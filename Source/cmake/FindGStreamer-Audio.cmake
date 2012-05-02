# - Try to find GStreamer-Audio
# Once done, this will define
#
#  GStreamer-Audio_FOUND - system has GStreamer-Audio
#  GStreamer-Audio_INCLUDE_DIRS - the GStreamer-Audio include directories
#  GStreamer-Audio_LIBRARIES - link these to use GStreamer-Audio

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-Audio GStreamer)
libfind_package(GStreamer-Audio GStreamer-Base)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-Audio_PKGCONF gstreamer-audio-0.10>=0.10.30)

# Include dir
find_path(GStreamer-Audio_INCLUDE_DIR
  NAMES gst/audio/audio.h
  PATHS ${GStreamer-Audio_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-Audio_LIBRARY
  NAMES gstaudio-0.10
  PATHS ${GStreamer-Audio_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-Audio_PROCESS_INCLUDES GStreamer-Audio_INCLUDE_DIR
GStreamer_INCLUDE_DIR GStreamer-Base_INCLUDE_DIR)
set(GStreamer-Audio_PROCESS_LIBS GStreamer-Audio_LIBRARY GStreamer_LIBRARIES GStreamer-Base_LIBRARIES)
libfind_process(GStreamer-Audio)
