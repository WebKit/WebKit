WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${Platform_INCLUDES})
add_library(Platform STATIC ${Platform_SOURCES})
set_target_properties(Platform PROPERTIES FOLDER "Platform")
