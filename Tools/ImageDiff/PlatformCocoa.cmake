if (WEBKIT_SDK_IS_MACOS)

find_package(Apple REQUIRED COMPONENTS CoreFoundation CoreGraphics CoreServices CoreText ImageIO)

include(CoreGraphics.cmake)

# CoreServices provides kUTTypePNG on macOS (guarded by __APPLE__ in PlatformImageCG.cpp).
list(APPEND ImageDiff_LIBRARIES
    Apple::CoreServices
)

# kUTTypePNG deprecated in 12.0; our deployment target is 26.2.
list(APPEND ImageDiff_COMPILE_OPTIONS -Wno-deprecated-declarations)

# FIXME: Remove once source files are fixed. https://bugs.webkit.org/show_bug.cgi?id=312034
list(APPEND ImageDiff_COMPILE_OPTIONS -Wno-unused-parameter)

elseif (WEBKIT_SDK_IS_IOS_FAMILY)

# run-webkit-tests runs ImageDiff on the host, so it has to be a macOS executable.
WEBKIT_XCRUN(_host_clangxx --sdk macosx --find clang++)
WEBKIT_XCRUN(_host_sdk --sdk macosx --show-sdk-path)

list(TRANSFORM ImageDiff_SOURCES PREPEND "${IMAGE_DIFF_DIR}/" OUTPUT_VARIABLE _image_diff_sources)
list(APPEND _image_diff_sources "${IMAGE_DIFF_DIR}/cg/PlatformImageCG.cpp")

if (WEBKIT_SDK_IS_SIMULATOR)
    # Match whatever architecture the simulated platform is using (e.g.
    # building for x86_64 and running through Rosetta).
    list(TRANSFORM CMAKE_OSX_ARCHITECTURES PREPEND "-arch;"
        OUTPUT_VARIABLE _imagediff_arch)
else ()
    set(_imagediff_arch "-arch" ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif ()

add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/ImageDiff"
    COMMAND "${_host_clangxx}" -std=c++2b -O3 -g -isysroot "${_host_sdk}" ${_imagediff_arch}
        -Wno-deprecated-declarations -I${IMAGE_DIFF_DIR} ${_image_diff_sources}
        -framework CoreFoundation -framework CoreGraphics -framework CoreServices -framework CoreText -framework ImageIO
        -o "${CMAKE_BINARY_DIR}/ImageDiff"
    DEPENDS ${_image_diff_sources} "${IMAGE_DIFF_DIR}/PlatformImage.h"
    COMMENT "Building ImageDiff for the host"
    VERBATIM)
add_custom_target(ImageDiff ALL DEPENDS "${CMAKE_BINARY_DIR}/ImageDiff")

endif ()
