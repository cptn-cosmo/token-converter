// SPDX-License-Identifier: GPL-3.0-or-later
#include "converterregistry.h"
#include "scanner.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

class ScannerTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;

    void touch(const QString &rel)
    {
        const QString p = m_dir->filePath(rel);
        QDir().mkpath(QFileInfo(p).absolutePath());
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    QStringList relative(const QStringList &files) const
    {
        QStringList out;
        for (const auto &f : files) {
            out << QDir(m_dir->path()).relativeFilePath(f);
        }
        return out;
    }

    ConversionSettings defaults() const
    {
        return Settings::toTyped(Settings::factoryDefaults());
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        touch(u"a.png"_s);
        touch(u"B.PNG"_s);
        touch(u"c.jpg"_s);
        touch(u"sub/d.png"_s);
        touch(u"sub/deep/e.png"_s);
        touch(u".git/objects/f.png"_s);
        touch(u"node_modules/pkg/g.png"_s);
        touch(u".token-converter-backup/old.png"_s);
        touch(u".a.webp.tctmp-1234"_s);
    }

    void collectsRecursivelyWithDefaults()
    {
        const QStringList files = relative(Scanner::collectFiles(m_dir->path(), defaults()));
        QCOMPARE(files, (QStringList{u"B.PNG"_s, u"a.png"_s, u"sub/d.png"_s, u"sub/deep/e.png"_s}));
    }

    void includeExcludePatterns()
    {
        ConversionSettings s = defaults();
        s.includePatterns = {u"sub/**"_s};
        s.excludePatterns = {u"e.*"_s};
        QCOMPARE(relative(Scanner::collectFiles(m_dir->path(), s)), QStringList{u"sub/d.png"_s});
    }

    void folderExclusionPrunes()
    {
        ConversionSettings s = defaults();
        s.excludeFolders = {u"deep"_s};
        const QStringList files = relative(Scanner::collectFiles(m_dir->path(), s));
        QVERIFY(files.contains(u".git/objects/f.png"_s)); // no longer excluded
        QVERIFY(!files.contains(u"sub/deep/e.png"_s));
        QVERIFY(!files.contains(u".token-converter-backup/old.png"_s)); // backup folder is always skipped
    }

    void symlinksOffByDefault()
    {
        QVERIFY(QFile::link(m_dir->filePath(u"sub"_s), m_dir->filePath(u"linked"_s)));
        QVERIFY(QFile::link(m_dir->filePath(u"."_s), m_dir->filePath(u"sub/loop"_s))); // cycle

        QVERIFY(!relative(Scanner::collectFiles(m_dir->path(), defaults())).contains(u"linked/d.png"_s));

        ConversionSettings s = defaults();
        s.followSymlinks = true;
        const QStringList files = relative(Scanner::collectFiles(m_dir->path(), s)); // must terminate
        QVERIFY(files.contains(u"a.png"_s));
        QCOMPARE(files.count(u"a.png"_s), 1);
    }

    void cancelStopsScan()
    {
        std::atomic_bool cancel{true};
        QVERIFY(Scanner::collectFiles(m_dir->path(), defaults(), &cancel).isEmpty());
    }

    void planResolvesInBatchCollisions()
    {
        touch(u"x.png"_s);
        touch(u"x.PNG"_s); // both want x.webp (on case-sensitive filesystems)
        ConversionSettings s = defaults();
        const QStringList files{m_dir->filePath(u"x.PNG"_s), m_dir->filePath(u"x.png"_s)};
        const auto tasks = Scanner::plan(m_dir->path(), files, s, ConverterRegistry::instance());
        QCOMPARE(tasks.size(), 2);
        QCOMPARE(QFileInfo(tasks[0].target).fileName(), u"x.webp"_s);
        QCOMPARE(QFileInfo(tasks[1].target).fileName(), u"x-1.webp"_s);
        QVERIFY(tasks[1].autoNumbered);

        s.collisionMode = CollisionMode::Skip;
        const auto skipped = Scanner::plan(m_dir->path(), files, s, ConverterRegistry::instance());
        QCOMPARE(skipped[1].state, PlannedTask::State::SkipTargetExists);
    }

    void planExistingTargets()
    {
        touch(u"a.webp"_s);
        ConversionSettings s = defaults();
        const QStringList files{m_dir->filePath(u"a.png"_s)};

        s.collisionMode = CollisionMode::Overwrite;
        auto t = Scanner::plan(m_dir->path(), files, s, ConverterRegistry::instance());
        QVERIFY(t[0].overwritesExisting);
        QCOMPARE(QFileInfo(t[0].target).fileName(), u"a.webp"_s);

        s.collisionMode = CollisionMode::AutoNumber;
        t = Scanner::plan(m_dir->path(), files, s, ConverterRegistry::instance());
        QCOMPARE(QFileInfo(t[0].target).fileName(), u"a-1.webp"_s);
    }

    void planTemplateAndCounter()
    {
        ConversionSettings s = defaults();
        s.prefix = u"web_"_s;
        s.filenameTemplate = u"{parent}_{counter:2}"_s;
        const QStringList files = Scanner::collectFiles(m_dir->path(), s);
        const auto tasks = Scanner::plan(m_dir->path(), files, s, ConverterRegistry::instance());
        QCOMPARE(QFileInfo(tasks.last().target).fileName(), u"web_deep_04.webp"_s);
        QCOMPARE(QFileInfo(tasks.last().target).absolutePath(), m_dir->filePath(u"sub/deep"_s));
    }

    void planImageSize()
    {
        QImage img(31, 17, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(m_dir->filePath(u"sized.png"_s)));
        ConversionSettings s = defaults();
        s.filenameTemplate = u"{name}_{width}x{height}"_s;
        const auto tasks = Scanner::plan(m_dir->path(), {m_dir->filePath(u"sized.png"_s)}, s, ConverterRegistry::instance());
        QCOMPARE(QFileInfo(tasks.first().target).fileName(), u"sized_31x17.webp"_s);
    }

    void backupPaths()
    {
        ConversionSettings s = defaults();
        QCOMPARE(Scanner::backupPathFor(u"/p"_s, u"/p/sub/a.png"_s, s), u"/p/.token-converter-backup/sub/a.png"_s);
        s.backupFolder = u"/elsewhere/bk"_s;
        QCOMPARE(Scanner::backupPathFor(u"/p"_s, u"/p/a.png"_s, s), u"/elsewhere/bk/a.png"_s);
    }
};

QTEST_MAIN(ScannerTest)
#include "scannertest.moc"
