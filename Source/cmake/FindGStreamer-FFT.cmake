# - Try to find GStreamer-FFT
# Once done, this will define
#
#  GStreamer-FFT_FOUND - system has GStreamer-FFT
#  GStreamer-FFT_INCLUDE_DIRS - the GStreamer-FFT include directories
#  GStreamer-FFT_LIBRARIES - link these to use GStreamer-FFT

include(LibFindMacros)

# Dependencies
libfind_package(GStreamer-FFT GStreamer)
libfind_package(GStreamer-FFT GStreamer-Base)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GStreamer-FFT_PKGCONF gstreamer-base-0.10>=0.10.30)

# Include dir
find_path(GStreamer-FFT_INCLUDE_DIR
  NAMES gst/fft/gstfftf32.h
  PATHS ${GStreamer-FFT_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES gstreamer-0.10
)

# Finally the library itself
find_library(GStreamer-FFT_LIBRARY
  NAMES gstfft-0.10
  PATHS ${GStreamer-FFT_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GStreamer-FFT_PROCESS_INCLUDES GStreamer-FFT_INCLUDE_DIR GStreamer_INCLUDE_DIR)
set(GStreamer-FFT_PROCESS_LIBS GStreamer-FFT_LIBRARY GStreamer_LIBRARIES)
libfind_process(GStreamer-FFT)
