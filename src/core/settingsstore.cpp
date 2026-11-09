// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsstore.h"
#include "settings.h"

#include <KConfigGroup>

#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace TokenConverter
{

static const QString SchemaVersionKey = u"SchemaVersion"_s;

SettingsStore::SettingsStore(KSharedConfigPtr config)
    : m_config(std::move(config))
{
}

QVariantMap SettingsStore::readSettings(const KConfigGroup &group)
{
    const int version = group.readEntry(SchemaVersionKey, 0);
    QVariantMap raw;
    for (const auto &def : Settings::definitions()) {
        if (!group.hasKey(def.key)) {
            continue;
        }
        switch (def.type) {
        case Settings::ValueType::Bool:
            raw[def.key] = group.readEntry(def.key, false);
            break;
        case Settings::ValueType::Int:
            raw[def.key] = group.readEntry(def.key, 0);
            break;
        case Settings::ValueType::String:
            raw[def.key] = group.readEntry(def.key, QString());
            break;
        case Settings::ValueType::StringList:
            raw[def.key] = group.readEntry(def.key, QStringList());
            break;
        case Settings::ValueType::Rules: {
            const QString text = group.readEntry(def.key, QString());
            const auto doc = QJsonDocument::fromJson(text.toUtf8());
            raw[def.key] = doc.isArray() ? QVariant(doc.toVariant()) : QVariant(text); // non-JSON => legacy, see migrate()
            break;
        }
        }
    }
    return Settings::migrate(raw, version);
}

void SettingsStore::writeSettings(KConfigGroup group, const QVariantMap &values)
{
    const QVariantMap m = Settings::normalized(values);
    group.writeEntry(SchemaVersionKey, Settings::SchemaVersion);
    for (const auto &def : Settings::definitions()) {
        const QVariant v = m.value(def.key);
        switch (def.type) {
        case Settings::ValueType::Bool:
            group.writeEntry(def.key, v.toBool());
            break;
        case Settings::ValueType::Int:
            group.writeEntry(def.key, v.toInt());
            break;
        case Settings::ValueType::String:
            group.writeEntry(def.key, v.toString());
            break;
        case Settings::ValueType::StringList:
            group.writeEntry(def.key, v.toStringList());
            break;
        case Settings::ValueType::Rules:
            group.writeEntry(def.key, QString::fromUtf8(QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact)));
            break;
        }
    }
}

QVariantMap SettingsStore::globalDefaults()
{
    KConfigGroup group(m_config, u"GlobalDefaults"_s);
    if (!group.exists() || !group.hasKey(SchemaVersionKey)) {
        const QVariantMap factory = Settings::factoryDefaults();
        writeSettings(group, factory);
        sync();
        return factory;
    }
    return readSettings(group);
}

void SettingsStore::setGlobalDefaults(const QVariantMap &values)
{
    writeSettings(KConfigGroup(m_config, u"GlobalDefaults"_s), values);
}

bool SettingsStore::globalAdvancedVisible() const
{
    return KConfigGroup(m_config, u"General"_s).readEntry("GlobalAdvancedVisible", false);
}

void SettingsStore::setGlobalAdvancedVisible(bool visible)
{
    KConfigGroup(m_config, u"General"_s).writeEntry("GlobalAdvancedVisible", visible);
}

QStringList SettingsStore::projectOrder() const
{
    return KConfigGroup(m_config, u"General"_s).readEntry("ProjectOrder", QStringList());
}

QList<ProjectRecord> SettingsStore::projects() const
{
    QList<ProjectRecord> out;
    const KConfigGroup projectsGroup(m_config, u"Project"_s);
    for (const QString &id : projectOrder()) {
        const KConfigGroup g = projectsGroup.group(id);
        if (!g.exists()) {
            continue;
        }
        ProjectRecord rec;
        rec.id = id;
        rec.name = g.readEntry("Name", QString());
        rec.path = g.readEntry("Path", QString());
        rec.advancedVisible = g.readEntry("AdvancedVisible", false);
        rec.settings = readSettings(g.group(u"Settings"_s));
        out << rec;
    }
    return out;
}

void SettingsStore::saveProject(const ProjectRecord &project)
{
    KConfigGroup g = KConfigGroup(m_config, u"Project"_s).group(project.id);
    g.writeEntry("Name", project.name);
    g.writeEntry("Path", project.path);
    g.writeEntry("AdvancedVisible", project.advancedVisible);
    writeSettings(g.group(u"Settings"_s), project.settings);

    QStringList order = projectOrder();
    if (!order.contains(project.id)) {
        order << project.id;
        KConfigGroup(m_config, u"General"_s).writeEntry("ProjectOrder", order);
    }
}

void SettingsStore::removeProject(const QString &id)
{
    KConfigGroup(m_config, u"Project"_s).group(id).deleteGroup();
    QStringList order = projectOrder();
    order.removeAll(id);
    KConfigGroup(m_config, u"General"_s).writeEntry("ProjectOrder", order);
}

void SettingsStore::sync()
{
    KConfigGroup(m_config, u"General"_s).writeEntry("SchemaVersion", Settings::SchemaVersion);
    m_config->sync();
}

} // namespace TokenConverter
