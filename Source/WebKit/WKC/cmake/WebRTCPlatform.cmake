# Wii U-only compatibility include path for the standalone libwebrtc bring-up.
#
# The workflow passes this file via CMAKE_PROJECT_libwebrtc_wiiu_INCLUDE. It is
# loaded after project() and before Tools/wiiu/libwebrtc adds the vendored
# libwebrtc subdirectory, so these includes are inherited by its targets.
get_filename_component(WKC_WIIU_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(WKC_WIIU_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

# Use the existing toolchain compatibility headers first. They are outside
# upstream-shared WebKit sources and are consumed only by this Wii U build.
include_directories(BEFORE
    "${WKC_WIIU_REPO_ROOT}/Tools/wiiu/libwebrtc/compat"
    "${WKC_WIIU_ROOT}/stubs"
)

# The Espresso CPU has no supported Abseil cycle-clock implementation. Disable
# it so timing falls back to the portable clock implementation.
add_compile_definitions(
    WKC_WIIU_WEBRTC=1
    ABSL_USE_UNSCALED_CYCLECLOCK=0
)
