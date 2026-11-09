// SPDX-License-Identifier: GPL-3.0-or-later

#include "appcontroller.h"
#include "converterregistry.h"
#include "tokenconverter-version.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml>

using namespace Qt::StringLiterals;
using namespace TokenConverter;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("token-converter");
    QApplication::setOrganizationDomain(u"geeks.gay"_s);
    QApplication::setWindowIcon(QIcon::fromTheme(u"token-converter"_s, QIcon(u":/token-converter.svg"_s)));
    QApplication::setDesktopFileName(u"gay.geeks.token-converter"_s);

    // Follow the platform style (Breeze via qqc2-desktop-style on Plasma).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    KAboutData about(u"token-converter"_s,
                     i18nc("@title", "Token Converter"),
                     QStringLiteral(TOKENCONVERTER_VERSION_STRING),
                     i18n("Recursively batch-convert media files in project folders"),
                     KAboutLicense::GPL_V3,
                     i18n("© 2026"));
    about.setDesktopFileName(u"gay.geeks.token-converter"_s);
    KAboutData::setApplicationData(about);

    // Make sure the built-in converters are registered before any UI exists.
    ConverterRegistry::instance();

    SettingsStore store;
    AppController controller(&store);

    qmlRegisterUncreatableType<ProjectSession>("org.kde.tokenconverter", 1, 0, "ProjectSession", u"Created by App"_s);
    qmlRegisterUncreatableType<SettingsController>("org.kde.tokenconverter", 1, 0, "SettingsController", u"Created by App"_s);
    qmlRegisterUncreatableType<JobRunner>("org.kde.tokenconverter", 1, 0, "JobRunner", u"Created by App"_s);
    qmlRegisterUncreatableType<ResultsFilterModel>("org.kde.tokenconverter", 1, 0, "ResultsFilterModel", u"Created by JobRunner"_s);
    qmlRegisterUncreatableType<PreviewModel>("org.kde.tokenconverter", 1, 0, "PreviewModel", u"Created by JobRunner"_s);
    qmlRegisterUncreatableType<ProjectListModel>("org.kde.tokenconverter", 1, 0, "ProjectListModel", u"Created by App"_s);
    qmlRegisterSingletonInstance("org.kde.tokenconverter", 1, 0, "App", &controller);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("org.kde.tokenconverter.ui", "Main");

#ifndef NDEBUG
    // Developer aid (debug builds only): TOKENCONVERTER_DEV_SCREENSHOTS="1500:/tmp/a.png;4000:/tmp/b.png"
    // grabs the window at the given times (ms) and quits after the last one.
    // Use with QT_QPA_PLATFORM=offscreen to render UI snapshots headlessly.
    if (const QString spec = qEnvironmentVariable("TOKENCONVERTER_DEV_SCREENSHOTS"); !spec.isEmpty() && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QStringList shots = spec.split(u';', Qt::SkipEmptyParts);
        int last = 0;
        for (const QString &shot : shots) {
            const qsizetype colon = shot.indexOf(u':');
            const int ms = shot.left(colon).toInt();
            const QString path = shot.mid(colon + 1);
            last = std::max(last, ms);
            QTimer::singleShot(ms, window, [window, path] {
                window->grabWindow().save(path);
            });
        }
        QTimer::singleShot(last + 200, &app, &QCoreApplication::quit);
    }
    // TOKENCONVERTER_DEV_VIEW=global shows the global defaults page;
    // TOKENCONVERTER_DEV_START=1 starts the first project shortly after launch.
    if (!engine.rootObjects().isEmpty()) {
        QObject *rootObject = engine.rootObjects().constFirst();
        if (qEnvironmentVariable("TOKENCONVERTER_DEV_VIEW") == u"global"_s) {
            QMetaObject::invokeMethod(rootObject, "showGlobal");
        }
        if (qEnvironmentVariableIsSet("TOKENCONVERTER_DEV_START")) {
            QTimer::singleShot(500, &controller, [&controller] {
                if (auto *s = controller.session(controller.projectIdAt(0))) {
                    s->start();
                }
            });
        }
    }
#endif

    const int ret = QApplication::exec();
    controller.flush();
    return ret;
}
