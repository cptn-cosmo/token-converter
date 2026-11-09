// SPDX-License-Identifier: GPL-3.0-or-later
#include "safewriter.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

#include <cerrno>
#include <cstdio>
#include <cstring>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

static const QString TempMarker = u".tctmp-"_s;

QString SafeWriter::tempPathFor(const QString &target)
{
    const QFileInfo fi(target);
    const QString random = QString::number(QRandomGenerator::global()->generate(), 16);
    return fi.absolutePath() + u"/."_s + fi.fileName() + TempMarker + random;
}

bool SafeWriter::isTempFile(const QString &fileName)
{
    return fileName.startsWith(u'.') && fileName.contains(TempMarker);
}

QString SafeWriter::autoNumbered(const QString &path, const std::function<bool(const QString &)> &taken)
{
    const auto isTaken = [&](const QString &p) {
        return QFileInfo::exists(p) || (taken && taken(p));
    };
    if (!isTaken(path)) {
        return path;
    }
    const QFileInfo fi(path);
    const QString dir = fi.absolutePath();
    const QString name = fi.fileName();
    const qsizetype dot = name.lastIndexOf(u'.');
    const QString base = dot > 0 ? name.left(dot) : name;
    const QString ext = dot > 0 ? name.mid(dot) : QString();
    for (int i = 1; i < 100000; ++i) {
        const QString candidate = dir + u'/' + base + u'-' + QString::number(i) + ext;
        if (!isTaken(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool SafeWriter::atomicReplace(const QString &from, const QString &to, QString *error)
{
    if (std::rename(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) != 0) {
        if (error) {
            *error = QString::fromLocal8Bit(std::strerror(errno));
        }
        return false;
    }
    return true;
}

bool SafeWriter::moveFile(const QString &from, const QString &to, QString *error)
{
    if (QFileInfo::exists(to)) {
        if (error) {
            *error = i18n("Destination already exists: %1", to);
        }
        return false;
    }
    QFile src(from);
    if (src.rename(to)) {
        return true;
    }
    // Cross-device: copy, then remove the source only once the copy is complete.
    QFile::remove(to);
    if (!src.copy(to)) {
        if (error) {
            *error = src.errorString();
        }
        QFile::remove(to);
        return false;
    }
    QFile copy(to);
    if (copy.size() != QFileInfo(from).size()) {
        QFile::remove(to);
        if (error) {
            *error = i18n("Copy is incomplete");
        }
        return false;
    }
    if (!QFile::remove(from)) {
        if (error) {
            *error = src.errorString();
        }
        // Both copies exist now; nothing is lost.
        return false;
    }
    return true;
}

SafeWriteOutcome SafeWriter::execute(const SafeWriteRequest &req, const Producer &produce, const Verifier &verify, const std::atomic_bool *cancel)
{
    SafeWriteOutcome out;
    const auto isCancelled = [cancel] {
        return cancel && cancel->load();
    };
    const auto cancelledOutcome = [&out]() {
        out.status = FileStatus::Skipped;
        out.cancelled = true;
        out.message = i18n("Cancelled");
        return out;
    };

    const QFileInfo srcInfo(req.source);
    if (!srcInfo.exists()) {
        out.message = i18n("Source file no longer exists");
        return out;
    }
    if (!srcInfo.isReadable()) {
        out.message = i18n("Permission denied: cannot read the source file");
        return out;
    }
    out.sizeBefore = srcInfo.size();

    const QString sourceAbs = srcInfo.absoluteFilePath();
    QString target = QFileInfo(req.target).absoluteFilePath();
    bool inPlace = target == sourceAbs;

    // In-place with "keep both" would destroy the original: number instead.
    if (inPlace && req.originalMode == OriginalMode::KeepBoth) {
        target = autoNumbered(target);
        inPlace = false;
    }

    if (!inPlace && QFileInfo::exists(target)) {
        switch (req.collisionMode) {
        case CollisionMode::Skip:
            out.status = FileStatus::Skipped;
            out.message = i18n("Target already exists: %1", QFileInfo(target).fileName());
            return out;
        case CollisionMode::AutoNumber:
            target = autoNumbered(target);
            if (target.isEmpty()) {
                out.message = i18n("Could not find a free file name");
                return out;
            }
            break;
        case CollisionMode::Overwrite:
            break;
        }
    }
    out.finalTarget = target;

    const QFileInfo targetDirInfo(QFileInfo(target).absolutePath());
    if (!targetDirInfo.isDir() && !QDir().mkpath(targetDirInfo.absoluteFilePath())) {
        out.message = i18n("Cannot create target folder: %1", targetDirInfo.absoluteFilePath());
        return out;
    }
    if (!QFileInfo(targetDirInfo.absoluteFilePath()).isWritable()) {
        out.message = i18n("Permission denied: folder is not writable");
        return out;
    }

    if (isCancelled()) {
        return cancelledOutcome();
    }

    // 1. produce into temp file
    const QString temp = tempPathFor(target);
    const ConvertResult result = produce(temp);
    if (result.cancelled || isCancelled()) {
        QFile::remove(temp);
        return cancelledOutcome();
    }
    if (!result.ok) {
        QFile::remove(temp);
        out.message = result.error.isEmpty() ? i18n("Conversion failed") : result.error;
        return out;
    }

    // 2. verify
    QString verifyError;
    if (!QFileInfo::exists(temp)) {
        out.message = i18n("Converter produced no output");
        return out;
    }
    if (verify && !verify(temp, &verifyError)) {
        QFile::remove(temp);
        out.message = i18n("Verification failed: %1", verifyError);
        return out;
    }
    out.sizeAfter = QFileInfo(temp).size();

    if (req.skipIfLarger && out.sizeAfter > out.sizeBefore) {
        QFile::remove(temp);
        out.status = FileStatus::Skipped;
        out.message = i18n("Result would be larger than the original");
        out.finalTarget.clear();
        return out;
    }
    if (isCancelled()) {
        QFile::remove(temp);
        return cancelledOutcome();
    }

    // Keep the original's permissions on the new file.
    QFile::setPermissions(temp, QFile::permissions(sourceAbs));

    // 3 + 4. commit and handle the original
    QString err;
    if (inPlace) {
        switch (req.originalMode) {
        case OriginalMode::Replace:
        case OriginalMode::KeepBoth: // unreachable, handled above
            break;
        case OriginalMode::Trash: {
            QString pathInTrash;
            if (!QFile::moveToTrash(sourceAbs, &pathInTrash)) {
                QFile::remove(temp);
                out.message = i18n("Could not move the original to the trash");
                return out;
            }
            if (!atomicReplace(temp, target, &err)) {
                // Put the original back where it was.
                if (!pathInTrash.isEmpty()) {
                    QFile::rename(pathInTrash, sourceAbs);
                }
                QFile::remove(temp);
                out.message = i18n("Could not write the converted file: %1", err);
                return out;
            }
            out.status = FileStatus::Converted;
            return out;
        }
        case OriginalMode::Backup: {
            const QString backup = autoNumbered(req.backupPath);
            QDir().mkpath(QFileInfo(backup).absolutePath());
            if (!QFile::copy(sourceAbs, backup)) {
                QFile::remove(temp);
                out.message = i18n("Could not back up the original to %1", backup);
                return out;
            }
            break;
        }
        }
        if (!atomicReplace(temp, target, &err)) {
            QFile::remove(temp);
            out.message = i18n("Could not write the converted file: %1", err);
            return out;
        }
        out.status = FileStatus::Converted;
        return out;
    }

    if (!atomicReplace(temp, target, &err)) {
        QFile::remove(temp);
        out.message = i18n("Could not write the converted file: %1", err);
        return out;
    }
    out.status = FileStatus::Converted;

    switch (req.originalMode) {
    case OriginalMode::KeepBoth:
        break;
    case OriginalMode::Replace:
        if (!QFile::remove(sourceAbs)) {
            out.message = i18n("Converted, but the original could not be removed");
        }
        break;
    case OriginalMode::Trash:
        if (!QFile::moveToTrash(sourceAbs)) {
            out.message = i18n("Converted, but the original could not be moved to the trash");
        }
        break;
    case OriginalMode::Backup: {
        const QString backup = autoNumbered(req.backupPath);
        QDir().mkpath(QFileInfo(backup).absolutePath());
        if (backup.isEmpty() || !moveFile(sourceAbs, backup, &err)) {
            out.message = i18n("Converted, but the original could not be moved to the backup folder: %1", err);
        }
        break;
    }
    }
    return out;
}

} // namespace TokenConverter
