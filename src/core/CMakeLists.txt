add_library(tokenconverter_core STATIC
    settings.cpp settings.h
    settingsstore.cpp settingsstore.h
    settingscontroller.cpp settingscontroller.h
    rulematcher.cpp rulematcher.h
    globfilter.cpp globfilter.h
    filenametemplate.cpp filenametemplate.h
    iconverter.h
    converterregistry.cpp converterregistry.h
    imageconverter.cpp imageconverter.h
    safewriter.cpp safewriter.h
    scanner.cpp scanner.h
    resultsmodel.cpp resultsmodel.h
    jobrunner.cpp jobrunner.h
)

target_include_directories(tokenconverter_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tokenconverter_core PRIVATE TRANSLATION_DOMAIN="token-converter")

target_link_libraries(tokenconverter_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        KF6::ConfigCore
        KF6::I18n
    PRIVATE
        PkgConfig::WEBP
)
