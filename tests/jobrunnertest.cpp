// SPDX-License-Identifier: GPL-3.0-or-later
// Integration tests: real PNG -> WebP conversions through the job runner.
#include "converterregistry.h"
#include "jobrunner.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <webp/decode.h>

using namespace Qt::StringLiterals;
using namespace TokenConverter;
namespace K = Settings::Keys;

class JobRunnerTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;

    void makePng(const QString &rel, int w = 64, int h = 48, bool noisy = false)
    {
        const QString p = m_dir->filePath(rel);
        QDir().mkpath(QFileInfo(p).absolutePath());
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(QColor(30, 120, 200, 200));
        if (noisy) {
            auto *rng = QRandomGenerator::global();
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    img.setPixel(x, y, rng->generate() | 0xff000000);
                }
            }
        }
        QVERIFY(img.save(p, "PNG"));
    }

    QVariantMap settings(std::initializer_list<std::pair<QString, QVariant>> overrides = {})
    {
        QVariantMap m = Settings::factoryDefaults();
        m[K::WorkerCount] = 4;
        for (const auto &[k, v] : overrides) {
            m[k] = v;
        }
        return m;
    }

    static bool isValidWebp(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QByteArray d = f.readAll();
        int w = 0, h = 0;
        return WebPGetInfo(reinterpret_cast<const uint8_t *>(d.constData()), size_t(d.size()), &w, &h) && w > 0 && h > 0;
    }

    static void waitFinished(JobRunner &runner)
    {
        if (runner.state() == JobRunner::State::Finished || runner.state() == JobRunner::State::Cancelled) {
            return;
        }
        QSignalSpy spy(&runner, &JobRunner::finished);
        QVERIFY(spy.wait(60000));
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void convertsRecursivelyAndReplaces()
    {
        makePng(u"a.png"_s);
        makePng(u"sub/b.PNG"_s);
        makePng(u"node_modules/skip.png"_s);

        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::SkipIfLarger, false}}));
        waitFinished(runner);

        QCOMPARE(runner.state(), JobRunner::State::Finished);
        QCOMPARE(runner.total(), 2);
        QCOMPARE(runner.processed(), 2);
        QCOMPARE(runner.convertedCount(), 2);
        QCOMPARE(runner.failedCount(), 0);
        QVERIFY(isValidWebp(m_dir->filePath(u"a.webp"_s)));
        QVERIFY(isValidWebp(m_dir->filePath(u"sub/b.webp"_s)));
        QVERIFY(!QFile::exists(m_dir->filePath(u"a.png"_s)));
        QVERIFY(!QFile::exists(m_dir->filePath(u"sub/b.PNG"_s)));
        QVERIFY(QFile::exists(m_dir->filePath(u"node_modules/skip.png"_s)));
        QCOMPARE(runner.resultsModel()->rowCount(), 2);
    }

    void lossyIsSmallerThanNoisyPng()
    {
        makePng(u"noise.png"_s, 200, 200, true);
        const qint64 before = QFileInfo(m_dir->filePath(u"noise.png"_s)).size();
        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::Quality, 50}, {K::OriginalMode, u"keep"_s}}));
        waitFinished(runner);
        QCOMPARE(runner.convertedCount(), 1);
        QCOMPARE(runner.bytesBefore(), before);
        QVERIFY(runner.savedBytes() > 0);
        QVERIFY(runner.savedPercent() > 0);
        QVERIFY(QFile::exists(m_dir->filePath(u"noise.png"_s)));
    }

    void losslessRoundTripsPixels()
    {
        makePng(u"exact.png"_s, 40, 30, true);
        QImage original(m_dir->filePath(u"exact.png"_s));
        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::Lossless, true}, {K::SkipIfLarger, false}, {K::OriginalMode, u"keep"_s}}));
        waitFinished(runner);
        QCOMPARE(runner.convertedCount(), 1);

        QFile f(m_dir->filePath(u"exact.webp"_s));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        int w = 0, h = 0;
        uint8_t *rgba = WebPDecodeRGBA(reinterpret_cast<const uint8_t *>(data.constData()), size_t(data.size()), &w, &h);
        QVERIFY(rgba);
        const QImage decoded = QImage(rgba, w, h, QImage::Format_RGBA8888).copy();
        WebPFree(rgba);
        QCOMPARE(decoded.convertToFormat(QImage::Format_ARGB32), original.convertToFormat(QImage::Format_ARGB32));
    }

    void corruptFileFailsAndIsKept()
    {
        QFile bad(m_dir->filePath(u"broken.png"_s));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("\x89PNG\r\n\x1a\nthis is not really a png");
        bad.close();
        makePng(u"good.png"_s);

        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::SkipIfLarger, false}}));
        waitFinished(runner);
        QCOMPARE(runner.failedCount(), 1);
        QCOMPARE(runner.convertedCount(), 1);
        QVERIFY(QFile::exists(m_dir->filePath(u"broken.png"_s)));
        QVERIFY(!QFile::exists(m_dir->filePath(u"broken.webp"_s)));
        runner.results()->setStatusFilter(u"failed"_s);
        QCOMPARE(runner.results()->rowCount(), 1);
        QVERIFY(!runner.results()->index(0, 0).data(ResultsModel::MessageRole).toString().isEmpty());
        // no temp files left behind
        QVERIFY(QDir(m_dir->path()).entryList({u".*tctmp*"_s}, QDir::Files | QDir::Hidden).isEmpty());
    }

    void dryRunTouchesNothingUntilConfirmed()
    {
        makePng(u"a.png"_s);
        JobRunner runner;
        QSignalSpy stateSpy(&runner, &JobRunner::stateChanged);
        runner.start(m_dir->path(), settings({{K::DryRun, true}, {K::SkipIfLarger, false}}));
        QTRY_COMPARE_WITH_TIMEOUT(runner.state(), JobRunner::State::AwaitingConfirmation, 10000);
        QCOMPARE(runner.preview()->rowCount(), 1);
        QCOMPARE(runner.preview()->index(0).data(PreviewModel::TargetNameRole).toString(), u"a.webp"_s);
        QVERIFY(QFile::exists(m_dir->filePath(u"a.png"_s)));
        QVERIFY(!QFile::exists(m_dir->filePath(u"a.webp"_s)));

        runner.rejectPreview();
        QCOMPARE(runner.state(), JobRunner::State::Idle);
        QVERIFY(QFile::exists(m_dir->filePath(u"a.png"_s)));

        runner.start(m_dir->path(), settings({{K::DryRun, true}, {K::SkipIfLarger, false}}));
        QTRY_COMPARE_WITH_TIMEOUT(runner.state(), JobRunner::State::AwaitingConfirmation, 10000);
        runner.confirmPreview();
        waitFinished(runner);
        QCOMPARE(runner.convertedCount(), 1);
        QVERIFY(isValidWebp(m_dir->filePath(u"a.webp"_s)));
    }

    void pauseAndResume()
    {
        for (int i = 0; i < 12; ++i) {
            makePng(u"p%1.png"_s.arg(i), 300, 300, true);
        }
        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::WorkerCount, 1}, {K::OriginalMode, u"keep"_s}}));
        QTRY_COMPARE_WITH_TIMEOUT(runner.state(), JobRunner::State::Running, 10000);
        runner.pause();
        QTRY_COMPARE_WITH_TIMEOUT(runner.state(), JobRunner::State::Paused, 10000);
        const int processedAtPause = runner.processed();
        QTest::qWait(300);
        QCOMPARE(runner.processed(), processedAtPause); // nothing progresses while paused
        QVERIFY(processedAtPause < 12);
        runner.resume();
        waitFinished(runner);
        QCOMPARE(runner.processed(), 12);
    }

    void cancelLeavesOriginalsIntact()
    {
        for (int i = 0; i < 16; ++i) {
            makePng(u"c%1.png"_s.arg(i), 600, 600, true);
        }
        JobRunner runner;
        runner.start(m_dir->path(), settings({{K::WorkerCount, 2}, {K::SkipIfLarger, false}}));
        QTRY_COMPARE_WITH_TIMEOUT(runner.state(), JobRunner::State::Running, 10000);
        runner.cancel();
        waitFinished(runner);
        QCOMPARE(runner.state(), JobRunner::State::Cancelled);
        QVERIFY(runner.processed() < 16);

        // Every original either still exists or has a valid converted result.
        for (int i = 0; i < 16; ++i) {
            const bool png = QFile::exists(m_dir->filePath(u"c%1.png"_s.arg(i)));
            const bool webp = QFile::exists(m_dir->filePath(u"c%1.webp"_s.arg(i)));
            QVERIFY(png || (webp && isValidWebp(m_dir->filePath(u"c%1.webp"_s.arg(i)))));
        }
        QVERIFY(QDir(m_dir->path()).entryList({u".*tctmp*"_s}, QDir::Files | QDir::Hidden).isEmpty());
    }

    void missingFolderReportsError()
    {
        JobRunner runner;
        runner.start(m_dir->filePath(u"does-not-exist"_s), settings());
        QCOMPARE(runner.state(), JobRunner::State::Idle);
        QVERIFY(!runner.errorString().isEmpty());
    }
};

QTEST_MAIN(JobRunnerTest)
#include "jobrunnertest.moc"
