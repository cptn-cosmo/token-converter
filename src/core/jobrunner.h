// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "resultsmodel.h"
#include "scanner.h"
#include "settings.h"

#include <QObject>
#include <QThreadPool>

#include <atomic>
#include <memory>

namespace TokenConverter
{

class ConverterRegistry;

/**
 * Drives one conversion run for a project: async scan/plan, optional dry-run
 * preview, then parallel execution on a private thread pool.
 *
 * All public methods must be called from the GUI thread. Workers report back
 * via queued invocations, so state is only ever mutated on the GUI thread.
 * Pause stops dispatching new files (in-flight ones finish); cancel also
 * aborts in-flight encodes, whose temp files are removed by SafeWriter.
 */
class JobRunner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(int processed READ processed NOTIFY progressChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY scanProgressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesBefore READ bytesBefore NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesAfter READ bytesAfter NOTIFY progressChanged)
    Q_PROPERTY(qint64 savedBytes READ savedBytes NOTIFY progressChanged)
    Q_PROPERTY(double savedPercent READ savedPercent NOTIFY progressChanged)
    Q_PROPERTY(int convertedCount READ convertedCount NOTIFY progressChanged)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY progressChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY progressChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(TokenConverter::ResultsFilterModel *results READ results CONSTANT)
    Q_PROPERTY(TokenConverter::PreviewModel *preview READ preview CONSTANT)

public:
    enum class State {
        Idle,
        Scanning,
        AwaitingConfirmation, ///< dry-run preview shown
        Running,
        Pausing, ///< waiting for in-flight files before Paused
        Paused,
        Cancelling, ///< waiting for in-flight files to abort
        Finished,
        Cancelled,
    };
    Q_ENUM(State)

    explicit JobRunner(QObject *parent = nullptr);
    JobRunner(ConverterRegistry &registry, QObject *parent = nullptr);
    ~JobRunner() override;

    State state() const;
    bool busy() const;
    int total() const;
    int processed() const;
    double progress() const;
    int scannedFiles() const;
    QString currentFile() const;
    qint64 bytesBefore() const;
    qint64 bytesAfter() const;
    qint64 savedBytes() const;
    double savedPercent() const;
    int convertedCount() const;
    int skippedCount() const;
    int failedCount() const;
    QString errorString() const;
    ResultsFilterModel *results() const;
    ResultsModel *resultsModel() const;
    PreviewModel *preview() const;

    /// Starts scanning @p root with a snapshot of @p settings.
    Q_INVOKABLE void start(const QString &root, const QVariantMap &settings);
    Q_INVOKABLE void confirmPreview();
    Q_INVOKABLE void rejectPreview();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void cancel();

    /// Blocks until all workers are done (for tests and shutdown).
    void waitForIdle();

Q_SIGNALS:
    void stateChanged();
    void progressChanged();
    void scanProgressChanged();
    void errorStringChanged();
    void finished();

private:
    void setState(State state);
    void setError(const QString &error);
    void onScanned(const QList<PlannedTask> &tasks, bool cancelled);
    void beginRun(const QList<PlannedTask> &tasks);
    void dispatch();
    void onTaskDone(const PlannedTask &task, const SafeWriteOutcome &outcome);
    void record(const FileResult &result);
    void checkSettled();
    void resetCounters();

    ConverterRegistry &m_registry;
    QThreadPool m_pool;
    State m_state = State::Idle;
    QString m_root;
    ConversionSettings m_settings;
    std::shared_ptr<std::atomic_bool> m_cancel;

    QList<PlannedTask> m_queue;
    qsizetype m_next = 0;
    int m_inFlight = 0;
    int m_processed = 0;
    int m_scanned = 0;
    QString m_currentFile;
    qint64 m_bytesBefore = 0;
    qint64 m_bytesAfter = 0;
    int m_converted = 0;
    int m_skipped = 0;
    int m_failed = 0;
    QString m_error;

    ResultsModel *m_resultsModel;
    ResultsFilterModel *m_results;
    PreviewModel *m_preview;
};

} // namespace TokenConverter
