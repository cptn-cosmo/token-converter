// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KSharedConfig>

#include <QList>
#include <QString>
#include <QVariantMap>

namespace TokenConverter
{

struct ProjectRecord {
    QString id;
    QString name;
    QString path;
    bool advancedVisible = false;
    QVariantMap settings;
};

/**
 * KConfig persistence (~/.config/token-converterrc).
 *
 *   [General]          SchemaVersion, GlobalAdvancedVisible, ProjectOrder
 *   [GlobalDefaults]   SchemaVersion, <setting keys>
 *   [Project][<id>]    Name, Path, AdvancedVisible
 *   [Project][<id>][Settings]  SchemaVersion, <setting keys>
 *
 * Every settings group carries its own SchemaVersion so it can be migrated
 * independently on load.
 */
class SettingsStore
{
public:
    explicit SettingsStore(KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("token-converterrc")));

    /// Seeds the global defaults from factory defaults on first launch.
    QVariantMap globalDefaults();
    void setGlobalDefaults(const QVariantMap &values);

    bool globalAdvancedVisible() const;
    void setGlobalAdvancedVisible(bool visible);

    QList<ProjectRecord> projects() const;
    void saveProject(const ProjectRecord &project);
    void removeProject(const QString &id);

    void sync();

    KSharedConfigPtr config() const
    {
        return m_config;
    }

private:
    static QVariantMap readSettings(const KConfigGroup &group);
    static void writeSettings(KConfigGroup group, const QVariantMap &values);
    QStringList projectOrder() const;

    KSharedConfigPtr m_config;
};

} // namespace TokenConverter
