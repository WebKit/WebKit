# - Find unifdef
# This module looks for unifdef. This module defines the
# following values:
#  UNIFDEF_EXECUTABLE: the full path to the unifdef tool.
#  UNIFDEF_FOUND: True if unifdef has been found.

find_program(UNIFDEF_EXECUTABLE unifdef)

# handle the QUIETLY and REQUIRED arguments and set UNIFDEF_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Unifdef DEFAULT_MSG UNIFDEF_EXECUTABLE)

mark_as_advanced(UNIFDEF_EXECUTABLE)
