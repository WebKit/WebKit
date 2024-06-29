cmake_minimum_required(VERSION 3.18)
project(qtwpe LANGUAGES CXX)

find_package(Qt5 REQUIRED COMPONENTS Core Quick Gui)
find_package(Qt5Test REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Epoxy REQUIRED IMPORTED_TARGET epoxy)
pkg_check_modules(WPEWebKit REQUIRED IMPORTED_TARGET wpe-webkit-1.0)
pkg_check_modules(WPEBackendFDO REQUIRED IMPORTED_TARGET wpebackend-fdo-1.0>=1.5.0)

add_library(qtwpe MODULE
    WPEQtViewBackend.cpp
    WPEQmlExtensionPlugin.cpp
    WPEQtView.cpp
    WPEQtViewLoadRequest.cpp
)
set_target_properties(qtwpe PROPERTIES
    OUTPUT_NAME qtwpe
    AUTOMOC ON
)
target_compile_definitions(qtwpe PUBLIC
    QT_NO_KEYWORDS=1
)
target_link_libraries(qtwpe PRIVATE
    PkgConfig::Epoxy
    PkgConfig::WPEWebKit
    PkgConfig::WPEBackendFDO
    Qt5::Core
    Qt5::Quick
)
target_include_directories(qtwpe PRIVATE
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
)

install(
    TARGETS
        qtwpe
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/qt5/qml/org/wpewebkit/qtwpe/"
    COMPONENT
        runtime
)
install(
    FILES
        qmldir
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/qt5/qml/org/wpewebkit/qtwpe/"
    COMPONENT
        runtime
)
install(
    FILES
        WPEQtView.h
        WPEQtViewLoadRequest.h
    DESTINATION
        include/wpe-webkit-1.0/wpe/qt
    COMPONENT
        development
)
