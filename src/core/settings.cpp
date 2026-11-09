// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"

#include <QRegularExpression>
#include <QThread>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace TokenConverter
{
namespace Settings
{

const QList<KeyDefinition> &definitions()
{
    static const QList<KeyDefinition> defs = {
        {Keys::Rules, ValueType::Rules},
        {Keys::Lossless, ValueType::Bool},
        {Keys::Quality, ValueType::Int},
        {Keys::OriginalMode, ValueType::String},
        {Keys::DryRun, ValueType::Bool},
        {Keys::SkipIfLarger, ValueType::Bool},
        {Keys::IncludePatterns, ValueType::StringList},
        {Keys::ExcludePatterns, ValueType::StringList},
        {Keys::ExcludeFolders, ValueType::StringList},
        {Keys::Prefix, ValueType::String},
        {Keys::Postfix, ValueType::String},
        {Keys::FilenameTemplate, ValueType::String},
        {Keys::CollisionMode, ValueType::String},
        {Keys::BackupFolder, ValueType::String},
        {Keys::WorkerCount, ValueType::Int},
        {Keys::PreserveMetadata, ValueType::Bool},
        {Keys::FollowSymlinks, ValueType::Bool},
    };
    return defs;
}

QStringList keys()
{
    QStringList result;
    for (const auto &def : definitions()) {
        result << def.key;
    }
    return result;
}

ValueType typeOf(const QString &key)
{
    for (const auto &def : definitions()) {
        if (def.key == key) {
            return def.type;
        }
    }
    return ValueType::String;
}

QVariantMap factoryDefaults()
{
    QVariantMap m;
    m[Keys::Rules] = rulesToVariant({ConversionRule{{u"png"_s}, u"webp"_s}});
    m[Keys::Lossless] = false;
    m[Keys::Quality] = 80;
    m[Keys::OriginalMode] = originalModeToString(OriginalMode::Replace);
    m[Keys::DryRun] = false;
    m[Keys::SkipIfLarger] = true;
    m[Keys::IncludePatterns] = QStringList{};
    m[Keys::ExcludePatterns] = QStringList{};
    m[Keys::ExcludeFolders] = QStringList{u".git"_s, u"node_modules"_s};
    m[Keys::Prefix] = QString();
    m[Keys::Postfix] = QString();
    m[Keys::FilenameTemplate] = QString();
    m[Keys::CollisionMode] = collisionModeToString(CollisionMode::AutoNumber);
    m[Keys::BackupFolder] = u".token-converter-backup"_s;
    m[Keys::WorkerCount] = std::max(1, QThread::idealThreadCount());
    m[Keys::PreserveMetadata] = true;
    m[Keys::FollowSymlinks] = false;
    return m;
}

QString normalizeExtension(const QString &ext)
{
    QString e = ext.trimmed().toLower();
    while (e.startsWith(u'*') || e.startsWith(u'.')) {
        e.remove(0, 1);
    }
    return e;
}

QVariantList rulesToVariant(const QList<ConversionRule> &rules)
{
    QVariantList list;
    for (const auto &rule : rules) {
        list << QVariantMap{{u"sources"_s, rule.sourceExtensions}, {u"target"_s, rule.targetFormat}};
    }
    return list;
}

static QStringList toStringList(const QVariant &v)
{
    if (v.typeId() == QMetaType::QString) {
        // Accept comma/space separated strings (e.g. from a text field)
        return v.toString().split(QRegularExpression(u"[,;\\s]+"_s), Qt::SkipEmptyParts);
    }
    QStringList out;
    const auto list = v.toList();
    for (const auto &item : list) {
        out << item.toString();
    }
    return out;
}

QList<ConversionRule> rulesFromVariant(const QVariant &value)
{
    QList<ConversionRule> rules;
    const auto list = value.toList();
    for (const auto &item : list) {
        const auto map = item.toMap();
        ConversionRule rule;
        const QStringList sources = toStringList(map.value(u"sources"_s));
        for (const auto &s : sources) {
            const QString ext = normalizeExtension(s);
            if (!ext.isEmpty() && !rule.sourceExtensions.contains(ext)) {
                rule.sourceExtensions << ext;
            }
        }
        rule.targetFormat = normalizeExtension(map.value(u"target"_s).toString());
        rules << rule;
    }
    return rules;
}

static QVariant coerce(ValueType type, const QVariant &v, const QVariant &fallback)
{
    if (!v.isValid()) {
        return fallback;
    }
    switch (type) {
    case ValueType::Bool:
        if (v.typeId() == QMetaType::QString) {
            return v.toString() == u"true"_s || v.toString() == u"1"_s;
        }
        return v.toBool();
    case ValueType::Int: {
        bool ok = false;
        const int i = v.toInt(&ok);
        return ok ? QVariant(i) : fallback;
    }
    case ValueType::String:
        return v.toString();
    case ValueType::StringList: {
        QStringList list;
        for (const auto &s : toStringList(v)) {
            if (!s.trimmed().isEmpty()) {
                list << s.trimmed();
            }
        }
        return list;
    }
    case ValueType::Rules:
        return rulesToVariant(rulesFromVariant(v));
    }
    return fallback;
}

QVariantMap normalized(const QVariantMap &values, const QVariantMap &fallback)
{
    QVariantMap out;
    for (const auto &def : definitions()) {
        const QVariant fb = fallback.contains(def.key) ? fallback.value(def.key) : factoryDefaults().value(def.key);
        out[def.key] = coerce(def.type, values.value(def.key), fb);
    }
    out[Keys::Quality] = std::clamp(out[Keys::Quality].toInt(), 0, 100);
    out[Keys::WorkerCount] = std::clamp(out[Keys::WorkerCount].toInt(), 1, 256);
    // Validate enumerations by round-tripping
    out[Keys::OriginalMode] = originalModeToString(originalModeFromString(out[Keys::OriginalMode].toString()));
    out[Keys::CollisionMode] = collisionModeToString(collisionModeFromString(out[Keys::CollisionMode].toString()));
    return out;
}

QVariantMap migrate(const QVariantMap &values, int fromVersion)
{
    QVariantMap m = values;
    // Version 0: pre-release layout that stored rules as "src1,src2>target;..."
    if (fromVersion < 1) {
        const QVariant rules = m.value(Keys::Rules);
        if (rules.typeId() == QMetaType::QString) {
            QVariantList list;
            const auto parts = rules.toString().split(u';', Qt::SkipEmptyParts);
            for (const auto &part : parts) {
                const auto halves = part.split(u'>');
                if (halves.size() == 2) {
                    list << QVariantMap{{u"sources"_s, halves[0].split(u',', Qt::SkipEmptyParts)}, {u"target"_s, halves[1]}};
                }
            }
            m[Keys::Rules] = list;
        }
    }
    // Future migrations go here: if (fromVersion < 2) { ... }

    // Any keys added since fromVersion are filled from the factory defaults.
    return normalized(m);
}

bool valuesEqual(const QString &key, const QVariant &a, const QVariant &b)
{
    switch (typeOf(key)) {
    case ValueType::Bool:
        return a.toBool() == b.toBool();
    case ValueType::Int:
        return a.toInt() == b.toInt();
    case ValueType::String:
        return a.toString() == b.toString();
    case ValueType::StringList:
        return toStringList(a) == toStringList(b);
    case ValueType::Rules:
        return rulesFromVariant(a) == rulesFromVariant(b);
    }
    return a == b;
}

QStringList differingKeys(const QVariantMap &a, const QVariantMap &b)
{
    QStringList out;
    for (const auto &def : definitions()) {
        if (!valuesEqual(def.key, a.value(def.key), b.value(def.key))) {
            out << def.key;
        }
    }
    return out;
}

QVariantMap withKeysFrom(const QVariantMap &target, const QVariantMap &source, const QStringList &keyList)
{
    QVariantMap out = target;
    for (const auto &key : keyList) {
        if (source.contains(key)) {
            out[key] = source.value(key);
        }
    }
    return normalized(out);
}

ConversionSettings toTyped(const QVariantMap &values)
{
    const QVariantMap m = normalized(values);
    ConversionSettings s;
    s.rules = rulesFromVariant(m[Keys::Rules]);
    s.lossless = m[Keys::Lossless].toBool();
    s.quality = m[Keys::Quality].toInt();
    s.originalMode = originalModeFromString(m[Keys::OriginalMode].toString());
    s.dryRun = m[Keys::DryRun].toBool();
    s.skipIfLarger = m[Keys::SkipIfLarger].toBool();
    s.includePatterns = m[Keys::IncludePatterns].toStringList();
    s.excludePatterns = m[Keys::ExcludePatterns].toStringList();
    s.excludeFolders = m[Keys::ExcludeFolders].toStringList();
    s.prefix = m[Keys::Prefix].toString();
    s.postfix = m[Keys::Postfix].toString();
    s.filenameTemplate = m[Keys::FilenameTemplate].toString();
    s.collisionMode = collisionModeFromString(m[Keys::CollisionMode].toString());
    s.backupFolder = m[Keys::BackupFolder].toString();
    s.workerCount = m[Keys::WorkerCount].toInt();
    s.preserveMetadata = m[Keys::PreserveMetadata].toBool();
    s.followSymlinks = m[Keys::FollowSymlinks].toBool();
    return s;
}

QString originalModeToString(OriginalMode mode)
{
    switch (mode) {
    case OriginalMode::Replace:
        return u"replace"_s;
    case OriginalMode::Trash:
        return u"trash"_s;
    case OriginalMode::Backup:
        return u"backup"_s;
    case OriginalMode::KeepBoth:
        return u"keep"_s;
    }
    return u"replace"_s;
}

OriginalMode originalModeFromString(const QString &s)
{
    if (s == u"trash"_s) {
        return OriginalMode::Trash;
    }
    if (s == u"backup"_s) {
        return OriginalMode::Backup;
    }
    if (s == u"keep"_s) {
        return OriginalMode::KeepBoth;
    }
    return OriginalMode::Replace;
}

QString collisionModeToString(CollisionMode mode)
{
    switch (mode) {
    case CollisionMode::Skip:
        return u"skip"_s;
    case CollisionMode::Overwrite:
        return u"overwrite"_s;
    case CollisionMode::AutoNumber:
        return u"autonumber"_s;
    }
    return u"autonumber"_s;
}

CollisionMode collisionModeFromString(const QString &s)
{
    if (s == u"skip"_s) {
        return CollisionMode::Skip;
    }
    if (s == u"overwrite"_s) {
        return CollisionMode::Overwrite;
    }
    return CollisionMode::AutoNumber;
}

} // namespace Settings

namespace Inheritance
{

QVariantMap snapshotForNewProject(const QVariantMap &globalDefaults)
{
    // A deep value copy: QVariantMap is implicitly shared but copy-on-write,
    // so later edits to the global map never reach the project.
    return Settings::normalized(globalDefaults);
}

QVariantMap resetAll(const QVariantMap &globalDefaults)
{
    return Settings::normalized(globalDefaults);
}

QVariantMap resetKey(const QVariantMap &project, const QVariantMap &globalDefaults, const QString &key)
{
    return Settings::withKeysFrom(project, globalDefaults, {key});
}

QStringList modifiedKeys(const QVariantMap &project, const QVariantMap &globalDefaults)
{
    return Settings::differingKeys(project, globalDefaults);
}

QVariantMap applyKeys(const QVariantMap &project, const QVariantMap &globalDefaults, const QStringList &keys)
{
    return Settings::withKeysFrom(project, globalDefaults, keys);
}

} // namespace Inheritance
} // namespace TokenConverter
