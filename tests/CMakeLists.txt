include(ECMAddTests)

ecm_add_tests(
    filenametemplatetest.cpp
    rulematchertest.cpp
    globfiltertest.cpp
    safewritertest.cpp
    settingstest.cpp
    scannertest.cpp
    jobrunnertest.cpp
    LINK_LIBRARIES tokenconverter_core Qt6::Test Qt6::Gui PkgConfig::WEBP
)
