qt_add_executable(token-converter
    main.cpp
    appcontroller.cpp appcontroller.h
)

target_include_directories(token-converter PRIVATE ${CMAKE_BINARY_DIR}/src)
target_compile_definitions(token-converter PRIVATE TRANSLATION_DOMAIN="token-converter")

set_source_files_properties(../../data/token-converter.svg PROPERTIES QT_RESOURCE_ALIAS token-converter.svg)
set_source_files_properties(qml/SettingLabels.js PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)

qt_add_qml_module(token-converter
    URI org.kde.tokenconverter.ui
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Sidebar.qml
        qml/WelcomePage.qml
        qml/ProjectPage.qml
        qml/GlobalDefaultsPage.qml
        qml/ProgressHeader.qml
        qml/ResultsView.qml
        qml/PreviewDialog.qml
        qml/SettingsForm.qml
        qml/RulesEditor.qml
        qml/ModifiedMarker.qml
        qml/ApplyToAllDialog.qml
        qml/SettingLabels.js
    RESOURCES
        ../../data/token-converter.svg
    RESOURCE_PREFIX /
    NO_RESOURCE_TARGET_PATH
)

target_link_libraries(token-converter PRIVATE
    tokenconverter_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::ConfigCore
)

install(TARGETS token-converter ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
