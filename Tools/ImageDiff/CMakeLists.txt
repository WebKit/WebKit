set(IMAGE_DIFF_DIR "${TOOLS_DIR}/ImageDiff")

set(IMAGE_DIFF_SYSTEM_INCLUDE_DIRECTORIES "")

set(IMAGE_DIFF_SOURCES
    ${IMAGE_DIFF_DIR}/ImageDiff.cpp
    ${IMAGE_DIFF_DIR}/PlatformImage.cpp
)

set(IMAGE_DIFF_INCLUDE_DIRECTORIES
    ${IMAGE_DIFF_DIR}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${IMAGE_DIFF_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${IMAGE_DIFF_SYSTEM_INCLUDE_DIRECTORIES})
add_executable(ImageDiff ${IMAGE_DIFF_SOURCES})
target_link_libraries(ImageDiff ${IMAGE_DIFF_LIBRARIES})

if (WIN32)
    add_dependencies(ImageDiff ImageDiffLib)
endif ()
