include_directories(./ PRIVATE ${JavaScriptCore_INCLUDE_DIRECTORIES} ${JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES})
add_library(jscLib SHARED ${JSC_SOURCES})

list(APPEND JSC_LIBRARIES
    Winmm
)

target_link_libraries(jscLib ${JSC_LIBRARIES})

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

set(JSC_SOURCES ${JAVASCRIPTCORE_DIR}/shell/DLLLauncherMain.cpp)
set(JSC_LIBRARIES shlwapi)
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

set(JSC_OUTPUT_NAME "jsc${DEBUG_SUFFIX}")

add_library(testRegExpLib SHARED ../testRegExp.cpp)
add_executable(testRegExp ${JSC_SOURCES})
set_target_properties(testRegExp PROPERTIES OUTPUT_NAME "testRegExp${DEBUG_SUFFIX}")
target_link_libraries(testRegExp shlwapi)
add_dependencies(testRegExp testRegExpLib)
target_link_libraries(testRegExpLib JavaScriptCore)

add_library(testapiLib SHARED ${TESTAPI_SOURCES})
set_source_files_properties(../API/tests/CustomGlobalObjectClassTest.c PROPERTIES COMPILE_FLAGS "/TP")
set_source_files_properties(../API/tests/testapi.c PROPERTIES COMPILE_FLAGS "/TP")
add_executable(testapi ${JSC_SOURCES})
set_target_properties(testapi PROPERTIES OUTPUT_NAME "testapi${DEBUG_SUFFIX}")
target_link_libraries(testapi shlwapi)
add_dependencies(testapi testapiLib)
target_link_libraries(testapiLib JavaScriptCore)

add_library(testmasmLib SHARED ../assembler/testmasm.cpp)
add_executable(testmasm ${JSC_SOURCES})
set_target_properties(testmasm PROPERTIES OUTPUT_NAME "testmasm${DEBUG_SUFFIX}")
target_link_libraries(testmasm shlwapi)
add_dependencies(testmasm testmasmLib)
target_link_libraries(testmasmLib JavaScriptCore)
