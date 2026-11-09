// SPDX-License-Identifier: GPL-3.0-or-later
#include "resultsmodel.h"
#include "rulematcher.h"

#include <KLocalizedString>

#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

// ---------------------------------------------------------------- ResultsModel

int ResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_results.size());
}

QString ResultsModel::statusString(FileStatus status)
{
    switch (status) {
    case FileStatus::Converted:
        return u"converted"_s;
    case FileStatus::Skipped:
        return u"skipped"_s;
    case FileStatus::Failed:
        return u"failed"_s;
    }
    return {};
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const FileResult &r = m_results.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case RelativePathRole:
        return r.relativePath;
    case TargetNameRole:
        return r.targetName;
    case StatusRole:
        return statusString(r.status);
    case SizeBeforeRole:
        return r.sizeBefore;
    case SizeAfterRole:
        return r.sizeAfter;
    case MessageRole:
        return r.message;
    }
    return {};
}

QHash<int, QByteArray> ResultsModel::roleNames() const
{
    return {
        {RelativePathRole, "relativePath"},
        {TargetNameRole, "targetName"},
        {StatusRole, "status"},
        {SizeBeforeRole, "sizeBefore"},
        {SizeAfterRole, "sizeAfter"},
        {MessageRole, "message"},
    };
}

void ResultsModel::clear()
{
    beginResetModel();
    m_results.clear();
    endResetModel();
    Q_EMIT countChanged();
}

void ResultsModel::append(const FileResult &result)
{
    const int row = int(m_results.size());
    beginInsertRows({}, row, row);
    m_results.append(result);
    endInsertRows();
    Q_EMIT countChanged();
}

// ---------------------------------------------------------- ResultsFilterModel

ResultsFilterModel::ResultsFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &ResultsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ResultsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ResultsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ResultsFilterModel::countChanged);
}

QString ResultsFilterModel::statusFilter() const
{
    return m_statusFilter;
}

void ResultsFilterModel::setStatusFilter(const QString &filter)
{
    if (filter == m_statusFilter) {
        return;
    }
    beginFilterChange();
    m_statusFilter = filter;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT statusFilterChanged();
    Q_EMIT countChanged();
}

int ResultsFilterModel::count() const
{
    return rowCount();
}

bool ResultsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_statusFilter.isEmpty()) {
        return true;
    }
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    return idx.data(ResultsModel::StatusRole).toString() == m_statusFilter;
}

// ---------------------------------------------------------------- PreviewModel

int PreviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_tasks.size());
}

QString PreviewModel::estimateFor(const PlannedTask &task) const
{
    switch (task.state) {
    case PlannedTask::State::NoConverter:
        return i18n("Fails: no converter for %1 → %2", RuleMatcher::extensionOf(task.relativePath).toUpper(), task.targetFormat.toUpper());
    case PlannedTask::State::SkipTargetExists:
        return i18n("Skipped: target already exists");
    case PlannedTask::State::Convert:
        break;
    }
    QString what = m_settings.lossless ? i18n("Convert to %1 (lossless)", task.targetFormat.toUpper())
                                       : i18n("Convert to %1 (quality %2)", task.targetFormat.toUpper(), m_settings.quality);
    if (task.overwritesExisting) {
        what += u" · "_s + i18n("overwrites existing file");
    } else if (task.autoNumbered) {
        what += u" · "_s + i18n("auto-numbered");
    }
    if (m_settings.skipIfLarger) {
        what += u" · "_s + i18n("skipped if larger");
    }
    return what;
}

QString PreviewModel::actionFor(const PlannedTask &task) const
{
    if (task.state != PlannedTask::State::Convert) {
        return i18n("Original left untouched");
    }
    if (task.target == task.source) {
        switch (m_settings.originalMode) {
        case OriginalMode::Trash:
            return i18n("Replace in place, original to trash");
        case OriginalMode::Backup:
            return i18n("Replace in place, original backed up");
        default:
            return i18n("Replace in place");
        }
    }
    switch (m_settings.originalMode) {
    case OriginalMode::Replace:
        return i18n("Replace original");
    case OriginalMode::Trash:
        return i18n("Move original to trash");
    case OriginalMode::Backup:
        return i18n("Move original to backup folder");
    case OriginalMode::KeepBoth:
        return i18n("Keep both");
    }
    return {};
}

QVariant PreviewModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const PlannedTask &t = m_tasks.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case RelativePathRole:
        return t.relativePath;
    case TargetNameRole:
        return t.target.isEmpty() ? QString() : QFileInfo(t.target).fileName();
    case EstimateRole:
        return estimateFor(t);
    case ActionRole:
        return actionFor(t);
    case WillConvertRole:
        return t.state == PlannedTask::State::Convert;
    }
    return {};
}

QHash<int, QByteArray> PreviewModel::roleNames() const
{
    return {
        {RelativePathRole, "relativePath"},
        {TargetNameRole, "targetName"},
        {EstimateRole, "estimate"},
        {ActionRole, "originalAction"},
        {WillConvertRole, "willConvert"},
    };
}

void PreviewModel::setPlan(const QList<PlannedTask> &tasks, const ConversionSettings &settings)
{
    beginResetModel();
    m_tasks = tasks;
    m_settings = settings;
    endResetModel();
    Q_EMIT countChanged();
}

void PreviewModel::clear()
{
    setPlan({}, m_settings);
}

int PreviewModel::convertCount() const
{
    return int(std::count_if(m_tasks.cbegin(), m_tasks.cend(), [](const PlannedTask &t) {
        return t.state == PlannedTask::State::Convert;
    }));
}

} // namespace TokenConverter
