set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT WIN_CAIRO)

include(Cairo.cmake)

WEBKIT_WRAP_EXECUTABLE(ImageDiff
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(ImageDiff PRIVATE ${wrapper_DEFINITIONS})
