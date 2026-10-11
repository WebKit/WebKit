# Wii U-only compatibility include path for the standalone libwebrtc bring-up.
#
# The workflow passes this file via CMAKE_PROJECT_libwebrtc_wiiu_INCLUDE. It is
# loaded after project() and before Tools/wiiu/libwebrtc adds the vendored
# libwebrtc subdirectory, so these includes are inherited by its targets.
get_filename_component(WKC_WIIU_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include_directories(BEFORE "${WKC_WIIU_ROOT}/stubs")
add_compile_definitions(WKC_WIIU_WEBRTC=1)
