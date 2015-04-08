set(WinLauncher_INCLUDE_DIRECTORIES
    WinLauncher.vcxproj
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit"
    "${DERIVED_SOURCES_DIR}"
    "${WTF_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
)

set(WinLauncher_SOURCES
    AccessibilityDelegate.cpp
    DOMDefaultImpl.cpp
    PageLoadTestClient.cpp
    PrintWebUIDelegate.cpp
    ResourceLoadDelegate.cpp
    WinLauncher.cpp
    WinLauncherWebHost.cpp
    WinMain.cpp
    stdafx.cpp

    WinLauncher.vcxproj/WinLauncherLib.rc
)

set(WinLauncher_LIBRARIES
    CFLite
    DbgHelp
    WTF
    WebKit
    WebKitGUID
    comctl32
    comsupp
    comsuppw
    shlwapi
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wWinMainCRTStartup")

add_definitions(-D_UNICODE -DWIN_CAIRO)
include_directories(${WinLauncher_INCLUDE_DIRECTORIES})
add_library(WinLauncher SHARED ${WinLauncher_SOURCES})
target_link_libraries(WinLauncher ${WinLauncher_LIBRARIES})
set_target_properties(WinLauncher PROPERTIES FOLDER "Tools")

add_executable(WinLauncherLauncher WIN32 ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp WinLauncher.vcxproj/WinLauncher.rc)
target_link_libraries(WinLauncherLauncher shlwapi)
set_target_properties(WinLauncherLauncher PROPERTIES FOLDER "Tools")
set_target_properties(WinLauncherLauncher PROPERTIES OUTPUT_NAME "WinLauncher")
