set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND wrapper_DEFINITIONS WIN_CAIRO)

    include(Cairo.cmake)
else ()
    include(CoreGraphics.cmake)
endif ()

WEBKIT_WRAP_EXECUTABLE(ImageDiff
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(ImageDiff PRIVATE ${wrapper_DEFINITIONS})
