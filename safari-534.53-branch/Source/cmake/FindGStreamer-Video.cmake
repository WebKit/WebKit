# - Try to find GStreamer-Video
# Once done, this will define
#
#  GStreamer-Video_FOUND - system has GStreamer-Video
#  GStreamer-Video_INCLUDE_DIRS - the GStreamer-Video include directories
#  GStreamer-Video_LIBRARIES - link these to use GStreamer-Video

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Video GStreamer)
libfind_package(GStreamer-Video GStreamer-Base)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Video_PKGCONF gstreamer-video-0.10)

# Include dir
find_path(GStreamer-Video_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-Video_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Video_LIBRARY
  NAMES gstvideo-0.10
  PATHS ${GStreamer-Video_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Video_PROCESS_INCLUDES GStreamer-Video_INCLUDE_DIR GStreamer_INCLUDE_DIR GStreamer-Base_INCLUDE_DIR)
set(GStreamer-Video_PROCESS_LIBS GStreamer-Video_LIBRARY GStreamer_LIBRARIES GStreamer-Base_LIBRARIES)
libfind_process(GStreamer-Video)
