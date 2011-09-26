# - Try to find Pango
# Once done, this will define
#
#  Pango_FOUND - system has Pango
#  Pango_INCLUDE_DIRS - the Pango include directories
#  Pango_LIBRARIES - link these to use Pango

include(LibFindMacros)

# Dependencies
libfind_package(Pango Freetype)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(Pango_PKGCONF pango)
libfind_pkg_check_modules(Pango_Cairo_PKGCONF pango)

# Include dir
find_path(Pango_INCLUDE_DIR
  NAMES pango/pango.h
  PATHS ${Pango_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES pango-1.0
)

find_path(Pango_Cairo_INCLUDE_DIR
  NAMES pango/pangocairo.h
  PATHS ${Pango_Cairo_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES pango-1.0
)

# Finally the library itself
find_library(Pango_LIBRARY
  NAMES pango-1.0
  PATHS ${Pango_PKGCONF_LIBRARY_DIRS}
)

find_library(Pango_Cairo_LIBRARY
  NAMES pangocairo-1.0
  PATHS ${Pango_Cairo_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(Pango_PROCESS_INCLUDES Pango_INCLUDE_DIR FREETYPE_INCLUDE_DIRS)
set(Pango_PROCESS_LIBS Pango_LIBRARY FREETYPE_LIBRARIES)
libfind_process(Pango)

