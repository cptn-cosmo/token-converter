// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "iconverter.h"
#include "settings.h"

#include <QString>

#include <atomic>
#include <functional>

namespace TokenConverter
{

enum class FileStatus {
    Converted,
    Skipped,
    Failed,
};

struct SafeWriteRequest {
    QString source; ///< absolute path of the original
    QString target; ///< desired absolute path of the converted file
    OriginalMode originalMode = OriginalMode::Replace;
    QString backupPath; ///< absolute destination for the original in Backup mode
    CollisionMode collisionMode = CollisionMode::AutoNumber;
    bool skipIfLarger = true;
};

struct SafeWriteOutcome {
    FileStatus status = FileStatus::Failed;
    QString finalTarget;
    qint64 sizeBefore = 0;
    qint64 sizeAfter = 0;
    QString message; ///< error, skip reason or warning
    bool cancelled = false;
};

/**
 * Implements the safe-write protocol:
 *
 *  1. produce the result into a temp file next to the target
 *  2. verify it
 *  3. atomically rename it into place (rename(2))
 *  4. only then dispose of the original according to OriginalMode
 *
 * The original is never modified before step 3 succeeded; any failure or
 * cancellation removes the temp file and leaves the original untouched.
 */
class SafeWriter
{
public:
    using Producer = std::function<ConvertResult(const QString &tempPath)>;
    using Verifier = std::function<bool(const QString &tempPath, QString *error)>;

    static SafeWriteOutcome execute(const SafeWriteRequest &request, const Producer &produce, const Verifier &verify, const std::atomic_bool *cancel = nullptr);

    /// Hidden temp file in the target's directory, e.g. ".photo.webp.tctmp-1a2b3c4d".
    static QString tempPathFor(const QString &target);
    static bool isTempFile(const QString &fileName);

    /// "dir/name.ext" -> first free "dir/name-1.ext", "dir/name-2.ext", ...
    static QString autoNumbered(const QString &path, const std::function<bool(const QString &)> &taken = {});

    /// rename(2): atomically replaces @p to if it exists. Same filesystem only.
    static bool atomicReplace(const QString &from, const QString &to, QString *error);

    /// Moves a file, falling back to copy + fsync + remove across filesystems.
    /// Never overwrites @p to.
    static bool moveFile(const QString &from, const QString &to, QString *error);
};

} // namespace TokenConverter
