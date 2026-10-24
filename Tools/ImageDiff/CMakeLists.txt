set(IMAGE_DIFF_DIR "${TOOLS_DIR}/ImageDiff")

set(ImageDiff_SOURCES
    ImageDiff.cpp
    PlatformImage.cpp
)

set(ImageDiff_PRIVATE_INCLUDE_DIRECTORIES
    ${IMAGE_DIFF_DIR}
)

# The embedded platforms run ImageDiff on the host, so PlatformCocoa.cmake builds it itself.
if (NOT WEBKIT_SDK_IS_IOS_FAMILY)
    WEBKIT_EXECUTABLE_DECLARE(ImageDiff)
endif ()
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
if (NOT WEBKIT_SDK_IS_IOS_FAMILY)
    WEBKIT_EXECUTABLE(ImageDiff)
endif ()
