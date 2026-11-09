// SPDX-License-Identifier: GPL-3.0-or-later
#include "jobrunner.h"

#include "converterregistry.h"
#include "safewriter.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace TokenConverter
{

JobRunner::JobRunner(QObject *parent)
    : JobRunner(ConverterRegistry::instance(), parent)
{
}

JobRunner::JobRunner(ConverterRegistry &registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_cancel(std::make_shared<std::atomic_bool>(false))
    , m_resultsModel(new ResultsModel(this))
    , m_results(new ResultsFilterModel(this))
    , m_preview(new PreviewModel(this))
{
    m_results->setSourceModel(m_resultsModel);
}

JobRunner::~JobRunner()
{
    m_cancel->store(true);
    m_pool.waitForDone();
}

JobRunner::State JobRunner::state() const
{
    return m_state;
}

bool JobRunner::busy() const
{
    switch (m_state) {
    case State::Scanning:
    case State::AwaitingConfirmation:
    case State::Running:
    case State::Pausing:
    case State::Paused:
    case State::Cancelling:
        return true;
    default:
        return false;
    }
}

int JobRunner::total() const
{
    return int(m_queue.size());
}

int JobRunner::processed() const
{
    return m_processed;
}

double JobRunner::progress() const
{
    return m_queue.isEmpty() ? 0.0 : double(m_processed) / double(m_queue.size());
}

int JobRunner::scannedFiles() const
{
    return m_scanned;
}

QString JobRunner::currentFile() const
{
    return m_currentFile;
}

qint64 JobRunner::bytesBefore() const
{
    return m_bytesBefore;
}

qint64 JobRunner::bytesAfter() const
{
    return m_bytesAfter;
}

qint64 JobRunner::savedBytes() const
{
    return m_bytesBefore - m_bytesAfter;
}

double JobRunner::savedPercent() const
{
    return m_bytesBefore > 0 ? 100.0 * double(savedBytes()) / double(m_bytesBefore) : 0.0;
}

int JobRunner::convertedCount() const
{
    return m_converted;
}

int JobRunner::skippedCount() const
{
    return m_skipped;
}

int JobRunner::failedCount() const
{
    return m_failed;
}

QString JobRunner::errorString() const
{
    return m_error;
}

ResultsFilterModel *JobRunner::results() const
{
    return m_results;
}

ResultsModel *JobRunner::resultsModel() const
{
    return m_resultsModel;
}

PreviewModel *JobRunner::preview() const
{
    return m_preview;
}

void JobRunner::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
    if (state == State::Finished || state == State::Cancelled) {
        m_currentFile.clear();
        Q_EMIT progressChanged();
        Q_EMIT finished();
    }
}

void JobRunner::setError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

void JobRunner::resetCounters()
{
    m_queue.clear();
    m_next = 0;
    m_processed = 0;
    m_scanned = 0;
    m_currentFile.clear();
    m_bytesBefore = 0;
    m_bytesAfter = 0;
    m_converted = 0;
    m_skipped = 0;
    m_failed = 0;
    m_resultsModel->clear();
    m_preview->clear();
    Q_EMIT progressChanged();
    Q_EMIT scanProgressChanged();
}

void JobRunner::start(const QString &root, const QVariantMap &settings)
{
    if (busy()) {
        return;
    }
    setError({});
    if (!QFileInfo(root).isDir()) {
        setError(i18n("The project folder does not exist: %1", root));
        return;
    }
    if (!QFileInfo(root).isReadable()) {
        setError(i18n("Permission denied: cannot read %1", root));
        return;
    }

    m_root = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    m_settings = Settings::toTyped(settings); // snapshot: later edits don't affect this run
    m_cancel = std::make_shared<std::atomic_bool>(false);
    resetCounters();
    m_pool.setMaxThreadCount(std::max(1, m_settings.workerCount));
    setState(State::Scanning);

    QPointer<JobRunner> self(this);
    const auto cancel = m_cancel;
    const QString root_ = m_root;
    const ConversionSettings settings_ = m_settings;
    ConverterRegistry *registry = &m_registry;

    m_pool.start([self, cancel, root_, settings_, registry] {
        auto progress = [self](int seen, const QString &) {
            QMetaObject::invokeMethod(
                self.get(),
                [self, seen] {
                    if (self) {
                        self->m_scanned = seen;
                        Q_EMIT self->scanProgressChanged();
                    }
                },
                Qt::QueuedConnection);
        };
        const QStringList files = Scanner::collectFiles(root_, settings_, cancel.get(), progress);
        QList<PlannedTask> tasks;
        if (!cancel->load()) {
            tasks = Scanner::plan(root_, files, settings_, *registry);
        }
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(
            self.get(),
            [self, tasks, cancelled] {
                if (self) {
                    self->onScanned(tasks, cancelled);
                }
            },
            Qt::QueuedConnection);
    });
}

void JobRunner::onScanned(const QList<PlannedTask> &tasks, bool cancelled)
{
    if (cancelled || m_state != State::Scanning) {
        setState(State::Cancelled);
        return;
    }
    if (m_settings.dryRun) {
        m_preview->setPlan(tasks, m_settings);
        setState(State::AwaitingConfirmation);
        return;
    }
    beginRun(tasks);
}

void JobRunner::confirmPreview()
{
    if (m_state != State::AwaitingConfirmation) {
        return;
    }
    const QList<PlannedTask> tasks = m_preview->tasks();
    beginRun(tasks);
}

void JobRunner::rejectPreview()
{
    if (m_state != State::AwaitingConfirmation) {
        return;
    }
    m_preview->clear();
    setState(State::Idle);
}

void JobRunner::beginRun(const QList<PlannedTask> &tasks)
{
    m_queue = tasks;
    m_next = 0;
    m_processed = 0;
    m_inFlight = 0;
    Q_EMIT progressChanged();
    setState(State::Running);
    if (m_queue.isEmpty()) {
        setState(State::Finished);
        return;
    }
    dispatch();
}

void JobRunner::dispatch()
{
    const int maxWorkers = std::max(1, m_settings.workerCount);
    while (m_state == State::Running && m_inFlight < maxWorkers && m_next < m_queue.size()) {
        const PlannedTask task = m_queue.at(m_next++);
        m_currentFile = task.relativePath;

        if (task.state == PlannedTask::State::SkipTargetExists) {
            ++m_processed;
            record({task.relativePath, QFileInfo(task.target).fileName(), FileStatus::Skipped, task.size, 0, i18n("Target already exists")});
            continue;
        }
        if (task.state == PlannedTask::State::NoConverter) {
            ++m_processed;
            record({task.relativePath, {}, FileStatus::Failed, task.size, 0, i18n("No converter available for this format")});
            continue;
        }
        const auto converter = m_registry.byId(task.converterId);
        if (!converter) {
            ++m_processed;
            record({task.relativePath, {}, FileStatus::Failed, task.size, 0, i18n("Converter \"%1\" is not available", task.converterId)});
            continue;
        }

        SafeWriteRequest req;
        req.source = task.source;
        req.target = task.target;
        req.originalMode = m_settings.originalMode;
        req.backupPath = Scanner::backupPathFor(m_root, task.source, m_settings);
        req.collisionMode = task.overwritesExisting ? CollisionMode::Overwrite : m_settings.collisionMode;
        req.skipIfLarger = m_settings.skipIfLarger;

        ConvertOptions opts;
        opts.lossless = m_settings.lossless;
        opts.quality = m_settings.quality;
        opts.preserveMetadata = m_settings.preserveMetadata;

        ++m_inFlight;
        QPointer<JobRunner> self(this);
        const auto cancel = m_cancel;
        m_pool.start([self, cancel, converter, req, opts, task] {
            ConvertOptions options = opts;
            options.cancelRequested = cancel.get();
            const QString format = task.targetFormat;
            const SafeWriteOutcome outcome = SafeWriter::execute(
                req,
                [&](const QString &temp) {
                    return converter->convert(req.source, temp, format, options);
                },
                [&](const QString &temp, QString *error) {
                    return converter->verify(temp, format, error);
                },
                cancel.get());
            QMetaObject::invokeMethod(
                self.get(),
                [self, task, outcome] {
                    if (self) {
                        self->onTaskDone(task, outcome);
                    }
                },
                Qt::QueuedConnection);
        });
    }
    Q_EMIT progressChanged();
    checkSettled();
}

void JobRunner::onTaskDone(const PlannedTask &task, const SafeWriteOutcome &outcome)
{
    --m_inFlight;
    if (outcome.cancelled) {
        // Aborted in flight: the original is untouched and the temp file was
        // removed; list it as skipped so the user sees what was interrupted.
        ++m_processed;
        record({task.relativePath, {}, FileStatus::Skipped, outcome.sizeBefore, 0, outcome.message});
    } else {
        ++m_processed;
        if (outcome.status == FileStatus::Converted) {
            m_bytesBefore += outcome.sizeBefore;
            m_bytesAfter += outcome.sizeAfter;
        }
        record({task.relativePath,
                outcome.status == FileStatus::Converted ? QFileInfo(outcome.finalTarget).fileName() : QString(),
                outcome.status,
                outcome.sizeBefore,
                outcome.sizeAfter,
                outcome.message});
    }
    if (m_state == State::Running) {
        dispatch();
    } else {
        Q_EMIT progressChanged();
        checkSettled();
    }
}

void JobRunner::record(const FileResult &result)
{
    switch (result.status) {
    case FileStatus::Converted:
        ++m_converted;
        break;
    case FileStatus::Skipped:
        ++m_skipped;
        break;
    case FileStatus::Failed:
        ++m_failed;
        break;
    }
    m_resultsModel->append(result);
}

void JobRunner::checkSettled()
{
    if (m_inFlight > 0) {
        return;
    }
    switch (m_state) {
    case State::Running:
        if (m_next >= m_queue.size()) {
            setState(State::Finished);
        }
        break;
    case State::Pausing:
        setState(State::Paused);
        break;
    case State::Cancelling:
        setState(State::Cancelled);
        break;
    default:
        break;
    }
}

void JobRunner::pause()
{
    if (m_state != State::Running) {
        return;
    }
    setState(State::Pausing);
    checkSettled();
}

void JobRunner::resume()
{
    if (m_state != State::Paused && m_state != State::Pausing) {
        return;
    }
    setState(State::Running);
    dispatch();
}

void JobRunner::cancel()
{
    switch (m_state) {
    case State::Scanning:
        m_cancel->store(true);
        // onScanned() completes the transition
        setState(State::Cancelling);
        break;
    case State::AwaitingConfirmation:
        rejectPreview();
        break;
    case State::Running:
    case State::Pausing:
    case State::Paused:
        m_cancel->store(true);
        setState(State::Cancelling);
        checkSettled();
        break;
    default:
        break;
    }
}

void JobRunner::waitForIdle()
{
    m_pool.waitForDone();
}

} // namespace TokenConverter
