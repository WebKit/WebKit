include_directories(./ ${JavaScriptCore_INCLUDE_DIRECTORIES})
add_library(jscLib SHARED ${JSC_SOURCES})

list(APPEND JSC_LIBRARIES
    Winmm
)

target_link_libraries(jscLib ${JSC_LIBRARIES})
set_target_properties(jscLib PROPERTIES FOLDER "JavaScriptCore")
set_target_properties(jscLib PROPERTIES OUTPUT_NAME "jsc${DEBUG_SUFFIX}")

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

set(JSC_SOURCES ${JAVASCRIPTCORE_DIR}/JavaScriptCore.vcxproj/jsc/DLLLauncherMain.cpp)
set(JSC_LIBRARIES shlwapi)
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

set(JSC_OUTPUT_NAME "jsc${DEBUG_SUFFIX}")
