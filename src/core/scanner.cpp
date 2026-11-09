// SPDX-License-Identifier: GPL-3.0-or-later
#include "scanner.h"

#include "converterregistry.h"
#include "filenametemplate.h"
#include "globfilter.h"
#include "rulematcher.h"
#include "safewriter.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace TokenConverter::Scanner
{

QString backupRoot(const QString &root, const ConversionSettings &settings)
{
    const QString folder = settings.backupFolder.trimmed().isEmpty() ? u".token-converter-backup"_s : settings.backupFolder.trimmed();
    const QString expanded = folder.startsWith(u"~/"_s) ? QDir::homePath() + folder.mid(1) : folder;
    return QDir::cleanPath(QDir(root).absoluteFilePath(expanded));
}

QString backupPathFor(const QString &root, const QString &source, const ConversionSettings &settings)
{
    const QString rel = QDir(root).relativeFilePath(source);
    return QDir::cleanPath(backupRoot(root, settings) + u'/' + rel);
}

namespace
{
struct WalkContext {
    QString root;
    QString backupDir;
    const ConversionSettings *settings;
    GlobFilter filter;
    RuleMatcher matcher;
    const std::atomic_bool *cancel;
    const ProgressFn *progress;
    QSet<QString> visitedDirs;
    QStringList out;
    int seen = 0;
};

void walk(WalkContext &ctx, const QString &dirPath)
{
    if (ctx.cancel && ctx.cancel->load()) {
        return;
    }
    if (ctx.settings->followSymlinks) {
        const QString canonical = QFileInfo(dirPath).canonicalFilePath();
        if (canonical.isEmpty() || ctx.visitedDirs.contains(canonical)) {
            return; // broken link or cycle
        }
        ctx.visitedDirs.insert(canonical);
    }

    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot | QDir::System, QDir::Name);
    for (const QFileInfo &fi : entries) {
        if (ctx.cancel && ctx.cancel->load()) {
            return;
        }
        if (fi.isSymLink() && !ctx.settings->followSymlinks) {
            continue;
        }
        const QString abs = fi.absoluteFilePath();
        const QString rel = QDir(ctx.root).relativeFilePath(abs);
        if (fi.isDir()) {
            if (QDir::cleanPath(abs) == ctx.backupDir) {
                continue;
            }
            if (!ctx.filter.acceptsDirectory(rel)) {
                continue;
            }
            walk(ctx, abs);
            continue;
        }
        if (!fi.isFile()) {
            continue;
        }
        ++ctx.seen;
        if (ctx.progress && *ctx.progress && (ctx.seen % 50) == 0) {
            (*ctx.progress)(ctx.seen, rel);
        }
        if (SafeWriter::isTempFile(fi.fileName())) {
            continue;
        }
        if (!ctx.matcher.match(fi.fileName())) {
            continue;
        }
        if (!ctx.filter.acceptsFile(rel)) {
            continue;
        }
        ctx.out << abs;
    }
}

QString describeParent(const QString &path)
{
    return QFileInfo(QFileInfo(path).absolutePath()).fileName();
}
} // namespace

QStringList collectFiles(const QString &root, const ConversionSettings &settings, const std::atomic_bool *cancel, const ProgressFn &progress)
{
    WalkContext ctx{
        QDir::cleanPath(QDir(root).absolutePath()),
        backupRoot(root, settings),
        &settings,
        GlobFilter(settings.includePatterns, settings.excludePatterns, settings.excludeFolders),
        RuleMatcher(settings.rules),
        cancel,
        &progress,
        {},
        {},
    };
    if (!QFileInfo(ctx.root).isDir()) {
        return {};
    }
    walk(ctx, ctx.root);
    std::sort(ctx.out.begin(), ctx.out.end());
    if (progress) {
        progress(ctx.seen, QString());
    }
    return ctx.out;
}

QList<PlannedTask> plan(const QString &root, const QStringList &files, const ConversionSettings &settings, const ConverterRegistry &registry)
{
    const RuleMatcher matcher(settings.rules);
    const bool needsSize = FilenameTemplate::usesImageSize(settings.filenameTemplate);
    QSet<QString> reserved;
    QList<PlannedTask> tasks;
    tasks.reserve(files.size());
    int counter = 0;
    const QDir rootDir(root);

    for (const QString &file : files) {
        const QFileInfo fi(file);
        const auto rule = matcher.match(fi.fileName());
        if (!rule) {
            continue;
        }
        PlannedTask task;
        task.source = fi.absoluteFilePath();
        task.relativePath = rootDir.relativeFilePath(task.source);
        task.size = fi.size();
        task.targetFormat = rule->targetFormat;

        const QString sourceExt = RuleMatcher::extensionOf(fi.fileName());
        const auto converter = registry.find(sourceExt, rule->targetFormat);
        if (!converter) {
            task.state = PlannedTask::State::NoConverter;
            tasks << task;
            continue;
        }
        task.converterId = converter->id();

        TemplateContext ctx;
        const QString name = fi.fileName();
        const qsizetype dot = name.lastIndexOf(u'.');
        ctx.name = dot > 0 ? name.left(dot) : name;
        ctx.ext = sourceExt;
        ctx.dateTime = fi.lastModified();
        ctx.counter = ++counter;
        ctx.parent = describeParent(task.source);
        if (needsSize) {
            const QSize size = QImageReader(task.source).size();
            ctx.width = size.width();
            ctx.height = size.height();
        }
        const QString fileName = FilenameTemplate::buildFileName(settings.prefix, settings.filenameTemplate, settings.postfix, ctx, rule->targetFormat);
        QString target = fi.absolutePath() + u'/' + fileName;

        const bool inPlace = target == task.source;
        const bool takenInBatch = reserved.contains(target);
        const bool existsOnDisk = !inPlace && QFileInfo::exists(target);

        if (inPlace && settings.originalMode == OriginalMode::KeepBoth) {
            target = SafeWriter::autoNumbered(target, [&](const QString &p) {
                return reserved.contains(p);
            });
            task.autoNumbered = true;
        } else if (takenInBatch || existsOnDisk) {
            if (settings.collisionMode == CollisionMode::Skip) {
                task.state = PlannedTask::State::SkipTargetExists;
            } else if (settings.collisionMode == CollisionMode::Overwrite && !takenInBatch) {
                task.overwritesExisting = true;
            } else {
                // AutoNumber, or Overwrite within the batch (never let two
                // sources clobber each other's result).
                target = SafeWriter::autoNumbered(target, [&](const QString &p) {
                    return reserved.contains(p);
                });
                task.autoNumbered = true;
            }
        }
        task.target = target;
        if (task.state == PlannedTask::State::Convert) {
            reserved.insert(target);
        }
        tasks << task;
    }
    return tasks;
}

} // namespace TokenConverter::Scanner
