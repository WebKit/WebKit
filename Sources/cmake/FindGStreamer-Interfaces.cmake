# - Try to find GStreamer-Interfaces
# Once done, this will define
#
#  GStreamer-Interfaces_FOUND - system has GStreamer-Interfaces
#  GStreamer-Interfaces_INCLUDE_DIRS - the GStreamer-Interfaces include directories
#  GStreamer-Interfaces_LIBRARIES - link these to use GStreamer-Interfaces

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Interfaces GStreamer)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Interfaces_PKGCONF gstreamer-interfaces-0.10)

# Include dir
find_path(GStreamer-Interfaces_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-Interfaces_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Interfaces_LIBRARY
  NAMES gstinterfaces-0.10
  PATHS ${GStreamer-Interfaces_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Interfaces_PROCESS_INCLUDES GStreamer-Interfaces_INCLUDE_DIR GStreamer_INCLUDE_DIR)
set(GStreamer-Interfaces_PROCESS_LIBS GStreamer-Interfaces_LIBRARY GStreamer_LIBRARIES)
libfind_process(GStreamer-Interfaces)
