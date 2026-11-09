// SPDX-License-Identifier: GPL-3.0-or-later
#include "appcontroller.h"

#include "settings.h"
#include "tokenconverter-version.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

// ---------------------------------------------------------------- ProjectSession

ProjectSession::ProjectSession(const ProjectRecord &record, const QVariantMap &globalDefaults, QObject *parent)
    : QObject(parent)
    , m_record(record)
    , m_folderExists(QFileInfo(record.path).isDir())
    , m_settings(new SettingsController(false, record.settings, globalDefaults, record.advancedVisible, this))
    , m_runner(new JobRunner(this))
{
    m_settings->setSaver([this](const QVariantMap &values, bool advanced) {
        m_record.settings = values;
        m_record.advancedVisible = advanced;
        Q_EMIT changed();
    });
}

QString ProjectSession::projectId() const
{
    return m_record.id;
}

QString ProjectSession::name() const
{
    return m_record.name;
}

void ProjectSession::setName(const QString &name)
{
    if (name == m_record.name) {
        return;
    }
    m_record.name = name;
    Q_EMIT nameChanged();
    Q_EMIT changed();
}

QString ProjectSession::path() const
{
    return m_record.path;
}

bool ProjectSession::folderExists() const
{
    return m_folderExists;
}

SettingsController *ProjectSession::settings() const
{
    return m_settings;
}

JobRunner *ProjectSession::runner() const
{
    return m_runner;
}

ProjectRecord ProjectSession::record() const
{
    return m_record;
}

void ProjectSession::start()
{
    refreshFolderState();
    m_runner->start(m_record.path, m_settings->values());
}

void ProjectSession::refreshFolderState()
{
    const bool exists = QFileInfo(m_record.path).isDir();
    if (exists != m_folderExists) {
        m_folderExists = exists;
        Q_EMIT folderExistsChanged();
    }
}

// -------------------------------------------------------------- ProjectListModel

int ProjectListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sessions.size());
}

QVariant ProjectListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    ProjectSession *s = m_sessions.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return s->name();
    case ProjectIdRole:
        return s->projectId();
    case PathRole:
        return s->path();
    case SessionRole:
        return QVariant::fromValue(s);
    }
    return {};
}

QHash<int, QByteArray> ProjectListModel::roleNames() const
{
    return {
        {ProjectIdRole, "projectId"},
        {NameRole, "name"},
        {PathRole, "path"},
        {SessionRole, "session"},
    };
}

void ProjectListModel::append(ProjectSession *session)
{
    const int row = int(m_sessions.size());
    beginInsertRows({}, row, row);
    m_sessions << session;
    endInsertRows();
    Q_EMIT countChanged();
}

void ProjectListModel::remove(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0) {
        return;
    }
    beginRemoveRows({}, row, row);
    m_sessions.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

void ProjectListModel::refresh(const QString &id)
{
    const int row = indexOf(id);
    if (row >= 0) {
        Q_EMIT dataChanged(index(row), index(row));
    }
}

int ProjectListModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i)->projectId() == id) {
            return i;
        }
    }
    return -1;
}

// ----------------------------------------------------------------- AppController

AppController::AppController(SettingsStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_projects(new ProjectListModel(this))
{
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(400);
    connect(&m_syncTimer, &QTimer::timeout, this, [this] {
        m_store->sync();
    });

    m_global = new SettingsController(true, m_store->globalDefaults(), Settings::factoryDefaults(), m_store->globalAdvancedVisible(), this);
    m_global->setSaver([this](const QVariantMap &values, bool advanced) {
        m_store->setGlobalDefaults(values);
        m_store->setGlobalAdvancedVisible(advanced);
        scheduleSync();
    });
    // Projects show "modified" relative to the *current* global defaults, but
    // their values never change when the globals do (snapshot semantics).
    connect(m_global, &SettingsController::valuesChanged, this, [this] {
        for (ProjectSession *s : m_projects->sessions()) {
            s->settings()->setReference(m_global->values());
        }
    });

    const auto records = m_store->projects();
    for (const ProjectRecord &rec : records) {
        m_projects->append(createSession(rec));
    }
}

AppController::~AppController()
{
    flush();
}

ProjectSession *AppController::createSession(const ProjectRecord &record)
{
    auto *s = new ProjectSession(record, m_global->values(), this);
    connect(s, &ProjectSession::changed, this, [this, s] {
        saveProject(s);
    });
    return s;
}

void AppController::saveProject(ProjectSession *session)
{
    m_store->saveProject(session->record());
    m_projects->refresh(session->projectId());
    scheduleSync();
}

void AppController::scheduleSync()
{
    m_syncTimer.start();
}

void AppController::flush()
{
    m_syncTimer.stop();
    m_store->sync();
}

ProjectListModel *AppController::projects() const
{
    return m_projects;
}

SettingsController *AppController::globalSettings() const
{
    return m_global;
}

QString AppController::version() const
{
    return QStringLiteral(TOKENCONVERTER_VERSION_STRING);
}

ProjectSession *AppController::session(const QString &id) const
{
    const int row = m_projects->indexOf(id);
    return row >= 0 ? m_projects->sessions().at(row) : nullptr;
}

QString AppController::localPath(const QUrl &folder) const
{
    return folder.isLocalFile() ? QDir::cleanPath(folder.toLocalFile()) : folder.toString();
}

QString AppController::folderName(const QUrl &folder) const
{
    const QString path = localPath(folder);
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? path : name;
}

QString AppController::addProject(const QUrl &folder, const QString &name)
{
    const QString path = localPath(folder);
    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        return {};
    }
    ProjectRecord rec;
    rec.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rec.name = name.trimmed().isEmpty() ? folderName(folder) : name.trimmed();
    rec.path = path;
    rec.advancedVisible = m_global->advancedVisible();
    rec.settings = Inheritance::snapshotForNewProject(m_global->values());

    ProjectSession *s = createSession(rec);
    m_projects->append(s);
    saveProject(s);
    return rec.id;
}

void AppController::renameProject(const QString &id, const QString &name)
{
    if (ProjectSession *s = session(id); s && !name.trimmed().isEmpty()) {
        s->setName(name.trimmed());
    }
}

void AppController::removeProject(const QString &id)
{
    ProjectSession *s = session(id);
    if (!s) {
        return;
    }
    s->runner()->cancel();
    m_projects->remove(id);
    m_store->removeProject(id);
    scheduleSync();
    s->deleteLater(); // JobRunner's destructor waits for its workers
}

void AppController::openFolder(const QString &id) const
{
    if (ProjectSession *s = session(id)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(s->path()));
    }
}

void AppController::applyToAllProjects(const QStringList &keys)
{
    if (keys.isEmpty()) {
        return;
    }
    const QVariantMap global = m_global->values();
    for (ProjectSession *s : m_projects->sessions()) {
        s->settings()->setValues(Inheritance::applyKeys(s->settings()->values(), global, keys));
    }
}

void AppController::restoreFactoryDefaults()
{
    m_global->resetAll(); // global's reference is the factory defaults
}

bool AppController::anyProjectBusy() const
{
    for (ProjectSession *s : m_projects->sessions()) {
        if (s->runner()->busy()) {
            return true;
        }
    }
    return false;
}

QString AppController::projectIdAt(int row) const
{
    const auto &sessions = m_projects->sessions();
    return row >= 0 && row < sessions.size() ? sessions.at(row)->projectId() : QString();
}

} // namespace TokenConverter
