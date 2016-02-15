set(MiniBrowser_INCLUDE_DIRECTORIES
    ../MiniBrowser.vcxproj
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit"
    "${WTF_DIR}"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
)

set(MiniBrowser_SOURCES
    AccessibilityDelegate.cpp
    DOMDefaultImpl.cpp
    PageLoadTestClient.cpp
    PrintWebUIDelegate.cpp
    ResourceLoadDelegate.cpp
    WebDownloadDelegate.cpp
    MiniBrowser.cpp
    MiniBrowserWebHost.cpp
    WinMain.cpp
    stdafx.cpp

    ../MiniBrowser.vcxproj/MiniBrowserLib.rc
)

set(MiniBrowser_LIBRARIES
    DbgHelp
    WTF
    WebKit
    WebKitGUID
    comctl32
    comsupp
    comsuppw
    shlwapi
)

if (WTF_PLATFORM_WIN_CAIRO)
    list(APPEND MiniBrowser_LIBRARIES
        CFLite
    )
else ()
    list(APPEND MiniBrowser_LIBRARIES
        CoreFoundation
    )
endif ()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wWinMainCRTStartup")

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()
add_definitions(-D_UNICODE)
include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
add_library(MiniBrowserLib SHARED ${MiniBrowser_SOURCES})
target_link_libraries(MiniBrowserLib ${MiniBrowser_LIBRARIES})
set_target_properties(MiniBrowserLib PROPERTIES FOLDER "Tools")

add_executable(MiniBrowser WIN32 ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp ../MiniBrowser.vcxproj/MiniBrowser.rc)
target_link_libraries(MiniBrowser shlwapi)
set_target_properties(MiniBrowser PROPERTIES FOLDER "Tools")
set_target_properties(MiniBrowser PROPERTIES OUTPUT_NAME "MiniBrowser")

add_dependencies(MiniBrowser MiniBrowserLib)
