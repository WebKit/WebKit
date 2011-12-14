# - Try to find GStreamer-Base
# Once done, this will define
#
#  GStreamer-Base_FOUND - system has GStreamer-Base
#  GStreamer-Base_INCLUDE_DIRS - the GStreamer-Base include directories
#  GStreamer-Base_LIBRARIES - link these to use GStreamer-Base

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Base GStreamer)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Base_PKGCONF gstreamer-base-0.10)

# Include dir
find_path(GStreamer-Base_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-Base_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Base_LIBRARY
  NAMES gstbase-0.10
  PATHS ${GStreamer-Base_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Base_PROCESS_INCLUDES GStreamer-Base_INCLUDE_DIR GStreamer_INCLUDE_DIR)
set(GStreamer-Base_PROCESS_LIBS GStreamer-Base_LIBRARY GStreamer_LIBRARIES)
libfind_process(GStreamer-Base)
