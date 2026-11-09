// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace TokenConverter
{

/// A single "source extensions -> target format" rule.
struct ConversionRule {
    QStringList sourceExtensions; ///< normalised: lower case, no leading dot
    QString targetFormat; ///< normalised: lower case, no leading dot

    bool operator==(const ConversionRule &) const = default;
};

enum class OriginalMode {
    Replace,
    Trash,
    Backup,
    KeepBoth,
};

enum class CollisionMode {
    Skip,
    Overwrite,
    AutoNumber,
};

/// Typed view over a settings map. Used by the conversion core; the UI and
/// persistence layers work on the (QML-friendly) QVariantMap representation.
struct ConversionSettings {
    QList<ConversionRule> rules;
    bool lossless = false;
    int quality = 80;
    OriginalMode originalMode = OriginalMode::Replace;
    bool dryRun = false;
    bool skipIfLarger = true;

    QStringList includePatterns;
    QStringList excludePatterns;
    QStringList excludeFolders;

    QString prefix;
    QString postfix;
    QString filenameTemplate;
    CollisionMode collisionMode = CollisionMode::AutoNumber;

    QString backupFolder;
    int workerCount = 1;
    bool preserveMetadata = true;
    bool followSymlinks = false;
};

/**
 * The settings schema. Settings are stored as a flat key -> value map so that
 * they can be diffed, reset and pushed per key (inheritance), persisted
 * generically and bound from QML.
 */
namespace Settings
{

/// Bump when keys are added/renamed/reinterpreted and extend migrate().
constexpr int SchemaVersion = 1;

namespace Keys
{
inline const QString Rules = QStringLiteral("rules");
inline const QString Lossless = QStringLiteral("lossless");
inline const QString Quality = QStringLiteral("quality");
inline const QString OriginalMode = QStringLiteral("originalMode");
inline const QString DryRun = QStringLiteral("dryRun");
inline const QString SkipIfLarger = QStringLiteral("skipIfLarger");
inline const QString IncludePatterns = QStringLiteral("includePatterns");
inline const QString ExcludePatterns = QStringLiteral("excludePatterns");
inline const QString ExcludeFolders = QStringLiteral("excludeFolders");
inline const QString Prefix = QStringLiteral("prefix");
inline const QString Postfix = QStringLiteral("postfix");
inline const QString FilenameTemplate = QStringLiteral("filenameTemplate");
inline const QString CollisionMode = QStringLiteral("collisionMode");
inline const QString BackupFolder = QStringLiteral("backupFolder");
inline const QString WorkerCount = QStringLiteral("workerCount");
inline const QString PreserveMetadata = QStringLiteral("preserveMetadata");
inline const QString FollowSymlinks = QStringLiteral("followSymlinks");
}

enum class ValueType {
    Bool,
    Int,
    String,
    StringList,
    Rules,
};

struct KeyDefinition {
    QString key;
    ValueType type;
};

/// All known keys in display order.
const QList<KeyDefinition> &definitions();
QStringList keys();
ValueType typeOf(const QString &key);

/// Built-in factory defaults (seed for the global defaults on first launch).
QVariantMap factoryDefaults();

/// Fills in missing keys from @p fallback, drops unknown keys and coerces
/// values to their declared type.
QVariantMap normalized(const QVariantMap &values, const QVariantMap &fallback = factoryDefaults());

/// Upgrades a map stored with @p fromVersion to the current schema.
QVariantMap migrate(const QVariantMap &values, int fromVersion);

bool valuesEqual(const QString &key, const QVariant &a, const QVariant &b);

/// Keys whose value differs between @p a and @p b.
QStringList differingKeys(const QVariantMap &a, const QVariantMap &b);

/// Returns @p target with the given keys copied from @p source.
QVariantMap withKeysFrom(const QVariantMap &target, const QVariantMap &source, const QStringList &keys);

ConversionSettings toTyped(const QVariantMap &values);

QVariantList rulesToVariant(const QList<ConversionRule> &rules);
QList<ConversionRule> rulesFromVariant(const QVariant &value);

/// Normalises an extension entered by a user (".PNG", "*.png", " png ") to "png".
QString normalizeExtension(const QString &ext);

QString originalModeToString(OriginalMode mode);
OriginalMode originalModeFromString(const QString &s);
QString collisionModeToString(CollisionMode mode);
CollisionMode collisionModeFromString(const QString &s);

} // namespace Settings

/**
 * Inheritance rules between global defaults and projects. Projects receive a
 * snapshot copy; there is never a live link.
 */
namespace Inheritance
{
QVariantMap snapshotForNewProject(const QVariantMap &globalDefaults);
QVariantMap resetAll(const QVariantMap &globalDefaults);
QVariantMap resetKey(const QVariantMap &project, const QVariantMap &globalDefaults, const QString &key);
QStringList modifiedKeys(const QVariantMap &project, const QVariantMap &globalDefaults);
QVariantMap applyKeys(const QVariantMap &project, const QVariantMap &globalDefaults, const QStringList &keys);
}

} // namespace TokenConverter
