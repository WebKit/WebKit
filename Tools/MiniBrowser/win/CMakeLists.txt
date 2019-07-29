set(MiniBrowser_INCLUDE_DIRECTORIES
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

set(MiniBrowser_SOURCES
    AccessibilityDelegate.cpp
    BrowserWindow.cpp
    Common.cpp
    DOMDefaultImpl.cpp
    MainWindow.cpp
    PrintWebUIDelegate.cpp
    ResourceLoadDelegate.cpp
    WebDownloadDelegate.cpp
    MiniBrowserLib.rc
    MiniBrowserWebHost.cpp
    WebKitLegacyBrowserWindow.cpp
    WinMain.cpp
    stdafx.cpp
)

set(MiniBrowser_LIBRARIES
    DbgHelp
    WebKit::WTF
    WebKitLegacy
    WebKitLegacyGUID
    comctl32
    comsupp
    comsuppw
    shlwapi
)

if (USE_CF)
    list(APPEND MiniBrowser_LIBRARIES
        ${COREFOUNDATION_LIBRARY}
    )
endif ()

if (ENABLE_WEBKIT)
    add_definitions(-DENABLE_WEBKIT)
    list(APPEND MiniBrowser_SOURCES
        WebKitBrowserWindow.cpp
    )
    list(APPEND MiniBrowser_LIBRARIES WebKit)
endif ()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wWinMainCRTStartup")

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()
add_definitions(-D_UNICODE)
include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
add_library(MiniBrowserLib SHARED ${MiniBrowser_SOURCES})
target_link_libraries(MiniBrowserLib ${MiniBrowser_LIBRARIES})

add_executable(MiniBrowser WIN32 ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp MiniBrowser.rc)
target_link_libraries(MiniBrowser shlwapi)
set_target_properties(MiniBrowser PROPERTIES OUTPUT_NAME "MiniBrowser")

add_dependencies(MiniBrowser MiniBrowserLib)
