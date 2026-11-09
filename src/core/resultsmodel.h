// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "safewriter.h"
#include "scanner.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

namespace TokenConverter
{

struct FileResult {
    QString relativePath;
    QString targetName;
    FileStatus status = FileStatus::Failed;
    qint64 sizeBefore = 0;
    qint64 sizeAfter = 0;
    QString message;
};

/// Per-file results of a run.
class ResultsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        RelativePathRole = Qt::UserRole + 1,
        TargetNameRole,
        StatusRole, ///< "converted" | "skipped" | "failed"
        SizeBeforeRole,
        SizeAfterRole,
        MessageRole,
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void append(const FileResult &result);

    static QString statusString(FileStatus status);

Q_SIGNALS:
    void countChanged();

private:
    QList<FileResult> m_results;
};

/// Filters ResultsModel by status ("" = all).
class ResultsFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ResultsFilterModel(QObject *parent = nullptr);

    QString statusFilter() const;
    void setStatusFilter(const QString &filter);
    int count() const;

Q_SIGNALS:
    void statusFilterChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_statusFilter;
};

/// Dry-run preview: what would happen to each file.
class PreviewModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int convertCount READ convertCount NOTIFY countChanged)

public:
    enum Roles {
        RelativePathRole = Qt::UserRole + 1,
        TargetNameRole,
        EstimateRole,
        ActionRole,
        WillConvertRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPlan(const QList<PlannedTask> &tasks, const ConversionSettings &settings);
    void clear();
    int convertCount() const;
    const QList<PlannedTask> &tasks() const
    {
        return m_tasks;
    }

Q_SIGNALS:
    void countChanged();

private:
    QString estimateFor(const PlannedTask &task) const;
    QString actionFor(const PlannedTask &task) const;

    QList<PlannedTask> m_tasks;
    ConversionSettings m_settings;
};

} // namespace TokenConverter
