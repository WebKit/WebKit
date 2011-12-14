# - Try to find GStreamer-Plugins-Base
# Once done, this will define
#
#  GStreamer-Plugins-Base_FOUND - system has GStreamer-Plugins-Base
#  GStreamer-Plugins-Base_INCLUDE_DIRS - the GStreamer-Plugins-Base include directories
#  GStreamer-Plugins-Base_LIBRARIES - link these to use GStreamer-Plugins-Base

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Plugins-Base GStreamer)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Plugins-Base_PKGCONF gstreamer-plugins-base-0.10)

# Include dir
find_path(GStreamer-Plugins-Base_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-Plugins-Base_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Plugins-Base_LIBRARY
  NAMES gstreamer-0.10
  PATHS ${GStreamer-Plugins-Base_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Plugins-Base_PROCESS_INCLUDES GStreamer-Plugins-Base_INCLUDE_DIR GStreamer_INCLUDE_DIR)
set(GStreamer-Plugins-Base_PROCESS_LIBS GStreamer-Plugins-Base_LIBRARY GStreamer_LIBRARIES)
libfind_process(GStreamer-Plugins-Base)
