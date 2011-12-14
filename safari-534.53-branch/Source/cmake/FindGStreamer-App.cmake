# - Try to find GStreamer-App
# Once done, this will define
#
#  GStreamer-App_FOUND - system has GStreamer
#  GStreamer-App_INCLUDE_DIRS - the GStreamer include directories
#  GStreamer-App_LIBRARIES - link these to use GStreamer

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-App GStreamer)
libfind_package(GStreamer-App GStreamer-Base)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-App_PKGCONF gstreamer-app-0.10)

# Include dir
find_path(GStreamer-App_INCLUDE_DIR
  NAMES gst/gst.h
  PATHS ${GStreamer-App_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-App_LIBRARY
  NAMES gstapp-0.10
  PATHS ${GStreamer-App_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-App_PROCESS_INCLUDES GStreamer-App_INCLUDE_DIR GStreamer_INCLUDE_DIR GStreamer-Base_INCLUDE_DIR)
set(GStreamer-App_PROCESS_LIBS GStreamer-App_LIBRARY GStreamer_LIBRARIES GStreamer-Base_LIBRARIES)
libfind_process(GStreamer-App)
