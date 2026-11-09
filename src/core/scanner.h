// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "settings.h"

#include <QList>
#include <QString>

#include <atomic>
#include <functional>

namespace TokenConverter
{

class ConverterRegistry;

struct PlannedTask {
    enum class State {
        Convert, ///< will be converted
        SkipTargetExists, ///< collision mode "skip" and target exists
        NoConverter, ///< no registered converter for the rule
    };

    QString source; ///< absolute
    QString relativePath; ///< relative to the project root, '/' separated
    QString target; ///< absolute, collisions already resolved
    QString targetFormat;
    QString converterId;
    qint64 size = 0;
    State state = State::Convert;
    bool overwritesExisting = false;
    bool autoNumbered = false;
};

/**
 * Recursive scanner + planner. Runs synchronously; the job runner calls it
 * from a worker thread. All functions are reentrant.
 */
namespace Scanner
{
using ProgressFn = std::function<void(int filesSeen, const QString &currentPath)>;

/// Recursively lists candidate files (absolute paths, sorted) honouring
/// include/exclude rules, folder exclusions, symlink policy and the rules.
QStringList collectFiles(const QString &root, const ConversionSettings &settings, const std::atomic_bool *cancel = nullptr, const ProgressFn &progress = {});

/// Computes target names (template, collisions, counters) for @p files.
QList<PlannedTask> plan(const QString &root, const QStringList &files, const ConversionSettings &settings, const ConverterRegistry &registry);

/// Absolute backup folder: relative paths are resolved against @p root.
QString backupRoot(const QString &root, const ConversionSettings &settings);

/// Where the original of @p source goes in Backup mode (mirrors the tree).
QString backupPathFor(const QString &root, const QString &source, const ConversionSettings &settings);
}

} // namespace TokenConverter
