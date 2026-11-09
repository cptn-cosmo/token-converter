cmake_minimum_required(VERSION 3.22)

project(token-converter VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.8.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMFindQmlModule)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX TOKENCONVERTER
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/tokenconverter-version.h")

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Concurrent Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami Config I18n CoreAddons)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp libwebpmux)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

qt_policy(SET QTP0001 NEW)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
