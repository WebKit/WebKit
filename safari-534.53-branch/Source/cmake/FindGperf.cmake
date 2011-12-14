# - Find gperf
# This module looks for gperf. This module defines the 
# following values:
#  GPERF_EXECUTABLE: the full path to the gperf tool.
#  GPERF_FOUND: True if gperf has been found.

INCLUDE(FindCygwin)

FIND_PROGRAM(GPERF_EXECUTABLE
    gperf
    ${CYGWIN_INSTALL_PATH}/bin
)

# handle the QUIETLY and REQUIRED arguments and set GPERF_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Gperf DEFAULT_MSG GPERF_EXECUTABLE)

MARK_AS_ADVANCED(GPERF_EXECUTABLE)
