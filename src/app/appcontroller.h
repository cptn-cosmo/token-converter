// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "jobrunner.h"
#include "settingscontroller.h"
#include "settingsstore.h"

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>

namespace TokenConverter
{

/// One project: its settings and its runner.
class ProjectSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString projectId READ projectId CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(bool folderExists READ folderExists NOTIFY folderExistsChanged)
    Q_PROPERTY(TokenConverter::SettingsController *settings READ settings CONSTANT)
    Q_PROPERTY(TokenConverter::JobRunner *runner READ runner CONSTANT)

public:
    ProjectSession(const ProjectRecord &record, const QVariantMap &globalDefaults, QObject *parent);

    QString projectId() const;
    QString name() const;
    void setName(const QString &name);
    QString path() const;
    bool folderExists() const;
    SettingsController *settings() const;
    JobRunner *runner() const;
    ProjectRecord record() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void refreshFolderState();

Q_SIGNALS:
    void nameChanged();
    void folderExistsChanged();
    void changed();

private:
    ProjectRecord m_record;
    bool m_folderExists;
    SettingsController *m_settings;
    JobRunner *m_runner;
};

class ProjectListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        ProjectIdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        SessionRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(ProjectSession *session);
    void remove(const QString &id);
    void refresh(const QString &id);
    int indexOf(const QString &id) const;
    const QList<ProjectSession *> &sessions() const
    {
        return m_sessions;
    }

Q_SIGNALS:
    void countChanged();

private:
    QList<ProjectSession *> m_sessions;
};

/// Root object exposed to QML as the "App" singleton.
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TokenConverter::ProjectListModel *projects READ projects CONSTANT)
    Q_PROPERTY(TokenConverter::SettingsController *globalSettings READ globalSettings CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit AppController(SettingsStore *store, QObject *parent = nullptr);
    ~AppController() override;

    ProjectListModel *projects() const;
    SettingsController *globalSettings() const;
    QString version() const;

    Q_INVOKABLE TokenConverter::ProjectSession *session(const QString &id) const;
    Q_INVOKABLE QString folderName(const QUrl &folder) const;
    Q_INVOKABLE QString localPath(const QUrl &folder) const;
    /// Returns the new project's id (empty on error).
    Q_INVOKABLE QString addProject(const QUrl &folder, const QString &name);
    Q_INVOKABLE void renameProject(const QString &id, const QString &name);
    /// Forgets the project. Never touches any files.
    Q_INVOKABLE void removeProject(const QString &id);
    Q_INVOKABLE void openFolder(const QString &id) const;
    /// Pushes the given keys of the global defaults to every project.
    Q_INVOKABLE void applyToAllProjects(const QStringList &keys);
    /// Global page: restore built-in factory defaults.
    Q_INVOKABLE void restoreFactoryDefaults();
    Q_INVOKABLE bool anyProjectBusy() const;
    Q_INVOKABLE QString projectIdAt(int row) const;

    void flush();

private:
    void scheduleSync();
    void saveProject(ProjectSession *session);
    ProjectSession *createSession(const ProjectRecord &record);

    SettingsStore *m_store;
    SettingsController *m_global;
    ProjectListModel *m_projects;
    QTimer m_syncTimer;
};

} // namespace TokenConverter
