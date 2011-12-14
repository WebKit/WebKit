# - Try to find GStreamer
# Once done, this will define
#
#  GStreamer_FOUND - system has GStreamer
#  GStreamer_INCLUDE_DIRS - the GStreamer include directories
#  GStreamer_LIBRARIES - link these to use GStreamer

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer_PKGCONF gstreamer-0.10)

# Include dir
find_path(GStreamer_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer_LIBRARY
  NAMES gstreamer-0.10
  PATHS ${GStreamer_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer_PROCESS_INCLUDES GStreamer_INCLUDE_DIR)
set(GStreamer_PROCESS_LIBS GStreamer_LIBRARY)
libfind_process(GStreamer)

