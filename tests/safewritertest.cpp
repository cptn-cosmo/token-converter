// SPDX-License-Identifier: GPL-3.0-or-later
#include "safewriter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

namespace
{
QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

SafeWriter::Producer writes(const QByteArray &data)
{
    return [data](const QString &temp) {
        QFile f(temp);
        if (!f.open(QIODevice::WriteOnly)) {
            return ConvertResult::failure(f.errorString());
        }
        f.write(data);
        return ConvertResult::success();
    };
}

bool acceptAll(const QString &, QString *)
{
    return true;
}

const QByteArray Original = QByteArray(1000, 'o');
const QByteArray Small = QByteArray(100, 's');
const QByteArray Large = QByteArray(5000, 'L');
}

class SafeWriterTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;

    QString path(const QString &rel) const
    {
        return m_dir->filePath(rel);
    }

    QStringList tempFiles() const
    {
        QStringList out;
        const auto entries = QDir(m_dir->path()).entryList(QDir::Files | QDir::Hidden);
        for (const auto &e : entries) {
            if (SafeWriter::isTempFile(e)) {
                out << e;
            }
        }
        return out;
    }

    SafeWriteRequest request(OriginalMode mode = OriginalMode::Replace) const
    {
        SafeWriteRequest r;
        r.source = path(u"a.png"_s);
        r.target = path(u"a.webp"_s);
        r.originalMode = mode;
        r.backupPath = path(u"backup/a.png"_s);
        return r;
    }

private Q_SLOTS:
    void init()
    {
        // Inside the build tree, so that trash tests use the user's home trash
        // device rather than a separate tmpfs.
        m_dir = std::make_unique<QTemporaryDir>(QDir::currentPath() + u"/safewriter-XXXXXX"_s);
        QVERIFY(m_dir->isValid());
        writeFile(path(u"a.png"_s), Original);
    }

    void cleanup()
    {
        QVERIFY2(tempFiles().isEmpty(), qPrintable(tempFiles().join(u", "_s)));
        m_dir.reset();
    }

    void tempNaming()
    {
        const QString t = SafeWriter::tempPathFor(path(u"x.webp"_s));
        QVERIFY(QFileInfo(t).fileName().startsWith(u".x.webp"_s));
        QVERIFY(SafeWriter::isTempFile(QFileInfo(t).fileName()));
        QCOMPARE(QFileInfo(t).absolutePath(), m_dir->path());
        QVERIFY(!SafeWriter::isTempFile(u"x.webp"_s));
    }

    void replaceOriginal()
    {
        const auto out = SafeWriter::execute(request(), writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Converted);
        QCOMPARE(out.sizeBefore, Original.size());
        QCOMPARE(out.sizeAfter, Small.size());
        QCOMPARE(readAll(path(u"a.webp"_s)), Small);
        QVERIFY(!QFile::exists(path(u"a.png"_s)));
    }

    void failedConversionKeepsOriginal()
    {
        const auto out = SafeWriter::execute(
            request(),
            [](const QString &temp) {
                writeFile(temp, "partial");
                return ConvertResult::failure(u"corrupt input"_s);
            },
            acceptAll);
        QCOMPARE(out.status, FileStatus::Failed);
        QCOMPARE(out.message, u"corrupt input"_s);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QVERIFY(!QFile::exists(path(u"a.webp"_s)));
    }

    void failedVerificationKeepsOriginal()
    {
        const auto out = SafeWriter::execute(request(), writes(Small), [](const QString &, QString *err) {
            *err = u"bad header"_s;
            return false;
        });
        QCOMPARE(out.status, FileStatus::Failed);
        QVERIFY(out.message.contains(u"bad header"_s));
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QVERIFY(!QFile::exists(path(u"a.webp"_s)));
    }

    void producerWithoutOutputFails()
    {
        const auto out = SafeWriter::execute(request(), [](const QString &) { return ConvertResult::success(); }, acceptAll);
        QCOMPARE(out.status, FileStatus::Failed);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
    }

    void skipIfLarger()
    {
        auto req = request();
        req.skipIfLarger = true;
        const auto out = SafeWriter::execute(req, writes(Large), acceptAll);
        QCOMPARE(out.status, FileStatus::Skipped);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QVERIFY(!QFile::exists(path(u"a.webp"_s)));

        req.skipIfLarger = false;
        QCOMPARE(SafeWriter::execute(req, writes(Large), acceptAll).status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.webp"_s)), Large);
    }

    void keepBoth()
    {
        const auto out = SafeWriter::execute(request(OriginalMode::KeepBoth), writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QCOMPARE(readAll(path(u"a.webp"_s)), Small);
    }

    void backupMovesOriginal()
    {
        const auto out = SafeWriter::execute(request(OriginalMode::Backup), writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Converted);
        QVERIFY(!QFile::exists(path(u"a.png"_s)));
        QCOMPARE(readAll(path(u"backup/a.png"_s)), Original);
        QCOMPARE(readAll(path(u"a.webp"_s)), Small);
    }

    void backupNeverOverwritesOlderBackup()
    {
        writeFile(path(u"backup/a.png"_s), "older backup");
        QCOMPARE(SafeWriter::execute(request(OriginalMode::Backup), writes(Small), acceptAll).status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"backup/a.png"_s)), QByteArray("older backup"));
        QCOMPARE(readAll(path(u"backup/a-1.png"_s)), Original);
    }

    void trashMovesOriginal()
    {
        const QString data = m_dir->filePath(u"xdg-data"_s);
        QDir().mkpath(data);
        qputenv("XDG_DATA_HOME", QFile::encodeName(data));
        const auto out = SafeWriter::execute(request(OriginalMode::Trash), writes(Small), acceptAll);
        qunsetenv("XDG_DATA_HOME");
        QCOMPARE(out.status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.webp"_s)), Small);
        if (!out.message.isEmpty()) {
            // No usable trash on this filesystem: the original must still be there.
            QVERIFY(QFile::exists(path(u"a.png"_s)));
        } else {
            QVERIFY(!QFile::exists(path(u"a.png"_s)));
        }
    }

    void collisionSkip()
    {
        writeFile(path(u"a.webp"_s), "existing");
        auto req = request();
        req.collisionMode = CollisionMode::Skip;
        const auto out = SafeWriter::execute(req, writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Skipped);
        QCOMPARE(readAll(path(u"a.webp"_s)), QByteArray("existing"));
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
    }

    void collisionOverwrite()
    {
        writeFile(path(u"a.webp"_s), "existing");
        auto req = request();
        req.collisionMode = CollisionMode::Overwrite;
        QCOMPARE(SafeWriter::execute(req, writes(Small), acceptAll).status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.webp"_s)), Small);
    }

    void collisionAutoNumber()
    {
        writeFile(path(u"a.webp"_s), "existing");
        writeFile(path(u"a-1.webp"_s), "existing too");
        auto req = request();
        req.collisionMode = CollisionMode::AutoNumber;
        const auto out = SafeWriter::execute(req, writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Converted);
        QCOMPARE(out.finalTarget, path(u"a-2.webp"_s));
        QCOMPARE(readAll(path(u"a.webp"_s)), QByteArray("existing"));
        QCOMPARE(readAll(path(u"a-2.webp"_s)), Small);
    }

    void inPlaceReplace()
    {
        auto req = request();
        req.target = req.source; // e.g. png -> png re-encode
        QCOMPARE(SafeWriter::execute(req, writes(Small), acceptAll).status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.png"_s)), Small);
    }

    void inPlaceBackup()
    {
        auto req = request(OriginalMode::Backup);
        req.target = req.source;
        QCOMPARE(SafeWriter::execute(req, writes(Small), acceptAll).status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.png"_s)), Small);
        QCOMPARE(readAll(path(u"backup/a.png"_s)), Original);
    }

    void inPlaceKeepBothNumbers()
    {
        auto req = request(OriginalMode::KeepBoth);
        req.target = req.source;
        const auto out = SafeWriter::execute(req, writes(Small), acceptAll);
        QCOMPARE(out.status, FileStatus::Converted);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QCOMPARE(readAll(path(u"a-1.png"_s)), Small);
    }

    void cancelRemovesTemp()
    {
        std::atomic_bool cancel{false};
        const auto out = SafeWriter::execute(
            request(),
            [&](const QString &temp) {
                writeFile(temp, "half written");
                cancel = true;
                return ConvertResult::aborted();
            },
            acceptAll,
            &cancel);
        QVERIFY(out.cancelled);
        QCOMPARE(out.status, FileStatus::Skipped);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QVERIFY(!QFile::exists(path(u"a.webp"_s)));
    }

    void cancelAfterProduceStillSafe()
    {
        std::atomic_bool cancel{false};
        const auto out = SafeWriter::execute(
            request(),
            [&](const QString &temp) {
                writeFile(temp, Small);
                cancel = true; // cancel arrives after a successful encode
                return ConvertResult::success();
            },
            acceptAll,
            &cancel);
        QVERIFY(out.cancelled);
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
        QVERIFY(!QFile::exists(path(u"a.webp"_s)));
    }

    void missingSource()
    {
        auto req = request();
        req.source = path(u"nope.png"_s);
        QCOMPARE(SafeWriter::execute(req, writes(Small), acceptAll).status, FileStatus::Failed);
    }

    void readOnlyFolder()
    {
        if (::geteuid() == 0) {
            QSKIP("root ignores permissions");
        }
        const QString ro = path(u"ro"_s);
        QDir().mkpath(ro);
        writeFile(ro + u"/b.png"_s, Original);
        QFile::setPermissions(ro, QFile::ReadOwner | QFile::ExeOwner);
        SafeWriteRequest req;
        req.source = ro + u"/b.png"_s;
        req.target = ro + u"/b.webp"_s;
        const auto out = SafeWriter::execute(req, writes(Small), acceptAll);
        QFile::setPermissions(ro, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QCOMPARE(out.status, FileStatus::Failed);
        QVERIFY(out.message.contains(u"ermission"_s));
        QCOMPARE(readAll(ro + u"/b.png"_s), Original);
    }

    void moveFileNeverOverwrites()
    {
        writeFile(path(u"dest.png"_s), "dest");
        QString err;
        QVERIFY(!SafeWriter::moveFile(path(u"a.png"_s), path(u"dest.png"_s), &err));
        QCOMPARE(readAll(path(u"dest.png"_s)), QByteArray("dest"));
        QCOMPARE(readAll(path(u"a.png"_s)), Original);
    }
};

QTEST_GUILESS_MAIN(SafeWriterTest)
#include "safewritertest.moc"
