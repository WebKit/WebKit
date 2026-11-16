# - Find xz
# This module looks for xz. This module defines the
# following variables:
#  XZ_EXECUTABLE
#  XZ_FOUND

include(FindCygwin)

find_program(XZ_EXECUTABLE
    xz
    ${CYGWIN_INSTALL_PATH}/bin
)

# Handle the QUIETLY and REQUIRED arguments and set XZ_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Xz DEFAULT_MSG XZ_EXECUTABLE)

mark_as_advanced(XZ_EXECUTABLE)
