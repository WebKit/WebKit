# - Try to find GStreamer-Pbutils
# Once done, this will define
#
#  GStreamer-Pbutils_FOUND - system has GStreamer-Pbutils
#  GStreamer-Pbutils_INCLUDE_DIRS - the GStreamer-Pbutils include directories
#  GStreamer-Pbutils_LIBRARIES - link these to use GStreamer-Pbutils

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Pbutils GStreamer)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Pbutils_PKGCONF gstreamer-pbutils-0.10)

# Include dir
find_path(GStreamer-Pbutils_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-Pbutils_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Pbutils_LIBRARY
  NAMES gstpbutils-0.10
  PATHS ${GStreamer-Pbutils_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Pbutils_PROCESS_INCLUDES GStreamer-Pbutils_INCLUDE_DIR GStreamer_INCLUDE_DIR)
set(GStreamer-Pbutils_PROCESS_LIBS GStreamer-Pbutils_LIBRARY GStreamer_LIBRARY)
libfind_process(GStreamer-Pbutils)
