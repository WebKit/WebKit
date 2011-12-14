# - Try to find the CFLite library
# Once done this will define
#
#  CFLITE_FOUND - System has CFLite
#  CFLITE_INCLUDE_DIR - The CFLite include directory
#  CFLITE_LIBRARIES - The libraries needed to use CFLite

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
FIND_PACKAGE(PkgConfig)

FIND_PATH(CFLITE_INCLUDE_DIR NAMES CoreFoundation/CoreFoundation.h)

FIND_LIBRARY(CFLITE_LIBRARIES NAMES CFLite.lib)

INCLUDE(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set COREFOUNDATION_FOUND to TRUE if
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CFLite DEFAULT_MSG CFLITE_LIBRARIES CFLITE_INCLUDE_DIR)

MARK_AS_ADVANCED(CFLITE_INCLUDE_DIR CFLITE_LIBRARIES)
