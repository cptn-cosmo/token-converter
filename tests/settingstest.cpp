// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "settingscontroller.h"
#include "settingsstore.h"

#include <KConfigGroup>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace TokenConverter;
namespace K = Settings::Keys;

class SettingsTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;

    KSharedConfigPtr freshConfig()
    {
        return KSharedConfig::openConfig(m_dir->filePath(u"token-converterrc"_s), KConfig::SimpleConfig);
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void factoryDefaults()
    {
        const ConversionSettings s = Settings::toTyped(Settings::factoryDefaults());
        QCOMPARE(s.rules.size(), 1);
        QCOMPARE(s.rules.first().sourceExtensions, QStringList{u"png"_s});
        QCOMPARE(s.rules.first().targetFormat, u"webp"_s);
        QCOMPARE(s.quality, 80);
        QCOMPARE(s.lossless, false);
        QCOMPARE(s.originalMode, OriginalMode::Replace);
        QCOMPARE(s.dryRun, false);
        QCOMPARE(s.skipIfLarger, true);
        QCOMPARE(s.excludeFolders, (QStringList{u".git"_s, u"node_modules"_s}));
        QCOMPARE(s.followSymlinks, false);
        QVERIFY(s.workerCount >= 1);
        QCOMPARE(Settings::factoryDefaults().keys().size(), Settings::keys().size());
    }

    void normalizeCoercesAndClamps()
    {
        QVariantMap m;
        m[K::Quality] = 250;
        m[K::Lossless] = u"true"_s;
        m[K::ExcludeFolders] = u".git, build  dist"_s;
        m[K::OriginalMode] = u"bogus"_s;
        m[u"unknownKey"_s] = 1;
        m[K::Rules] = QVariantList{QVariantMap{{u"sources"_s, u".PNG, *.Jpg"_s}, {u"target"_s, u"WEBP"_s}}};
        const QVariantMap n = Settings::normalized(m);
        QCOMPARE(n[K::Quality].toInt(), 100);
        QCOMPARE(n[K::Lossless].toBool(), true);
        QCOMPARE(n[K::ExcludeFolders].toStringList(), (QStringList{u".git"_s, u"build"_s, u"dist"_s}));
        QCOMPARE(n[K::OriginalMode].toString(), u"replace"_s);
        QVERIFY(!n.contains(u"unknownKey"_s));
        const auto rules = Settings::rulesFromVariant(n[K::Rules]);
        QCOMPARE(rules.first().sourceExtensions, (QStringList{u"png"_s, u"jpg"_s}));
        QCOMPARE(rules.first().targetFormat, u"webp"_s);
        // Missing keys are filled from the defaults
        QCOMPARE(n[K::SkipIfLarger].toBool(), true);
    }

    void migrateFromV0()
    {
        QVariantMap old;
        old[K::Rules] = u"png,bmp>webp;tif>jpg"_s;
        old[K::Quality] = 60;
        const QVariantMap m = Settings::migrate(old, 0);
        const auto rules = Settings::rulesFromVariant(m[K::Rules]);
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules[0].sourceExtensions, (QStringList{u"png"_s, u"bmp"_s}));
        QCOMPARE(rules[1].targetFormat, u"jpg"_s);
        QCOMPARE(m[K::Quality].toInt(), 60);
        QCOMPARE(m[K::FollowSymlinks].toBool(), false); // added key gets its default
    }

    void snapshotIsNotALiveLink()
    {
        QVariantMap global = Settings::factoryDefaults();
        const QVariantMap project = Inheritance::snapshotForNewProject(global);
        global[K::Quality] = 10;
        QCOMPARE(project[K::Quality].toInt(), 80);
        QVERIFY(Inheritance::modifiedKeys(project, global).contains(K::Quality));
    }

    void resetKeyAndAll()
    {
        QVariantMap global = Settings::factoryDefaults();
        global[K::Quality] = 50;
        QVariantMap project = Inheritance::snapshotForNewProject(global);
        project[K::Quality] = 90;
        project[K::Lossless] = true;
        QCOMPARE(Inheritance::modifiedKeys(project, global), (QStringList{K::Lossless, K::Quality}));

        project = Inheritance::resetKey(project, global, K::Quality);
        QCOMPARE(project[K::Quality].toInt(), 50);
        QCOMPARE(project[K::Lossless].toBool(), true);
        QCOMPARE(Inheritance::modifiedKeys(project, global), QStringList{K::Lossless});

        project = Inheritance::resetAll(global);
        QVERIFY(Inheritance::modifiedKeys(project, global).isEmpty());
    }

    void applySelectedKeys()
    {
        QVariantMap global = Settings::factoryDefaults();
        QVariantMap project = Inheritance::snapshotForNewProject(global);
        project[K::Prefix] = u"p_"_s;
        global[K::Quality] = 33;
        global[K::Lossless] = true;
        project = Inheritance::applyKeys(project, global, {K::Quality});
        QCOMPARE(project[K::Quality].toInt(), 33);
        QCOMPARE(project[K::Lossless].toBool(), false); // not selected
        QCOMPARE(project[K::Prefix].toString(), u"p_"_s); // untouched
    }

    void rulesCompareSemantically()
    {
        const QVariant a = QVariantList{QVariantMap{{u"sources"_s, QStringList{u"PNG"_s}}, {u"target"_s, u"webp"_s}}};
        const QVariant b = QVariantList{QVariantMap{{u"sources"_s, QStringList{u"png"_s}}, {u"target"_s, u".webp"_s}}};
        QVERIFY(Settings::valuesEqual(K::Rules, a, b));
    }

    void storeSeedsGlobalsOnFirstLaunch()
    {
        SettingsStore store(freshConfig());
        QVERIFY(Settings::differingKeys(store.globalDefaults(), Settings::factoryDefaults()).isEmpty());
        QVERIFY(KConfigGroup(store.config(), u"GlobalDefaults"_s).hasKey("SchemaVersion"));
    }

    void storeRoundTrip()
    {
        QVariantMap settings = Settings::factoryDefaults();
        settings[K::Quality] = 42;
        settings[K::ExcludePatterns] = QStringList{u"*_raw.png"_s, u"tmp/**"_s};
        settings[K::Rules] = QVariantList{QVariantMap{{u"sources"_s, QStringList{u"png"_s, u"bmp"_s}}, {u"target"_s, u"webp"_s}},
                                          QVariantMap{{u"sources"_s, QStringList{u"tif"_s}}, {u"target"_s, u"jpg"_s}}};
        {
            SettingsStore store(freshConfig());
            QVariantMap global = store.globalDefaults();
            global[K::DryRun] = true;
            store.setGlobalDefaults(global);
            store.saveProject({u"p1"_s, u"One"_s, u"/tmp/one"_s, true, settings});
            store.saveProject({u"p2"_s, u"Two"_s, u"/tmp/two"_s, false, Settings::factoryDefaults()});
            store.sync();
        }
        SettingsStore store(freshConfig());
        QCOMPARE(store.globalDefaults()[K::DryRun].toBool(), true);
        const auto projects = store.projects();
        QCOMPARE(projects.size(), 2);
        QCOMPARE(projects[0].id, u"p1"_s);
        QCOMPARE(projects[0].name, u"One"_s);
        QCOMPARE(projects[0].advancedVisible, true);
        QVERIFY(Settings::differingKeys(projects[0].settings, settings).isEmpty());

        store.removeProject(u"p1"_s);
        QCOMPARE(store.projects().size(), 1);
        QCOMPARE(store.projects().first().id, u"p2"_s);
    }

    void storeMigratesOldGroups()
    {
        auto config = freshConfig();
        KConfigGroup g = KConfigGroup(config, u"Project"_s).group(u"old"_s);
        g.writeEntry("Name", u"Legacy"_s);
        g.writeEntry("Path", u"/tmp/legacy"_s);
        KConfigGroup s = g.group(u"Settings"_s);
        s.writeEntry("SchemaVersion", 0);
        s.writeEntry("rules", u"gif>webp"_s);
        s.writeEntry("quality", 70);
        KConfigGroup(config, u"General"_s).writeEntry("ProjectOrder", QStringList{u"old"_s});
        config->sync();

        SettingsStore store(freshConfig());
        const auto projects = store.projects();
        QCOMPARE(projects.size(), 1);
        const ConversionSettings typed = Settings::toTyped(projects.first().settings);
        QCOMPARE(typed.rules.first().sourceExtensions, QStringList{u"gif"_s});
        QCOMPARE(typed.quality, 70);
        QCOMPARE(typed.excludeFolders, (QStringList{u".git"_s, u"node_modules"_s}));
    }

    void controllerTracksModifiedAgainstReference()
    {
        QVariantMap global = Settings::factoryDefaults();
        SettingsController project(false, global, global, false);
        QVariantMap saved;
        project.setSaver([&](const QVariantMap &v, bool) {
            saved = v;
        });
        QSignalSpy modifiedSpy(&project, &SettingsController::modifiedKeysChanged);
        QVERIFY(project.modifiedKeys().isEmpty());

        project.setValue(K::Quality, 55);
        QCOMPARE(project.modifiedKeys(), QStringList{K::Quality});
        QCOMPARE(saved[K::Quality].toInt(), 55);
        QCOMPARE(modifiedSpy.count(), 1);

        // Global changes don't alter values, only the indicator.
        global[K::Quality] = 55;
        project.setReference(global);
        QCOMPARE(project.values()[K::Quality].toInt(), 55);
        QVERIFY(project.modifiedKeys().isEmpty());

        global[K::Lossless] = true;
        project.setReference(global);
        QCOMPARE(project.values()[K::Lossless].toBool(), false);
        QCOMPARE(project.modifiedKeys(), QStringList{K::Lossless});

        project.resetValue(K::Lossless);
        QCOMPARE(project.values()[K::Lossless].toBool(), true);
        QVERIFY(project.modifiedKeys().isEmpty());
    }

    void controllerIgnoresUnknownKeysAndBadValues()
    {
        SettingsController c(true, Settings::factoryDefaults(), Settings::factoryDefaults(), false);
        c.setValue(u"nope"_s, 1);
        QVERIFY(!c.values().contains(u"nope"_s));
        c.setValue(K::Quality, u"not a number"_s);
        QCOMPARE(c.values()[K::Quality].toInt(), 80);
    }

    void controllerFilenamePreview()
    {
        QVariantMap m = Settings::factoryDefaults();
        m[K::Prefix] = u"web_"_s;
        m[K::FilenameTemplate] = u"{parent}-{name}-{width}"_s;
        SettingsController c(true, m, m, false);
        QCOMPARE(c.exampleFileName(), u"holiday-photo.png"_s);
        QCOMPARE(c.previewFileName(u"holiday-photo.png"_s), u"web_vacation-holiday-photo-1920.webp"_s);
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "settingstest.moc"
