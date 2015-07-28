set(WinLauncher_INCLUDE_DIRECTORIES
    WinLauncher.vcxproj
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_WEBKIT_DIR}/Interfaces"
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
    WebDownloadDelegate.cpp
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

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()
add_definitions(-D_UNICODE)
include_directories(${WinLauncher_INCLUDE_DIRECTORIES})
add_library(WinLauncherLib SHARED ${WinLauncher_SOURCES})
target_link_libraries(WinLauncherLib ${WinLauncher_LIBRARIES})
set_target_properties(WinLauncherLib PROPERTIES FOLDER "Tools")
set_target_properties(WinLauncherLib PROPERTIES OUTPUT_NAME "WinLauncher")

add_executable(WinLauncher WIN32 ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp WinLauncher.vcxproj/WinLauncher.rc)
target_link_libraries(WinLauncher shlwapi)
set_target_properties(WinLauncher PROPERTIES FOLDER "Tools")
set_target_properties(WinLauncher PROPERTIES OUTPUT_NAME "WinLauncher")
