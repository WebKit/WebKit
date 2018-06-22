if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)

    include(Cairo.cmake)
else ()
    include(PlatformMac.cmake)
endif ()

add_library(ImageDiffLib SHARED ${IMAGE_DIFF_SOURCES})
target_link_libraries(ImageDiffLib ${IMAGE_DIFF_LIBRARIES})

add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

set(IMAGE_DIFF_SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp)
set(IMAGE_DIFF_LIBRARIES shlwapi)
