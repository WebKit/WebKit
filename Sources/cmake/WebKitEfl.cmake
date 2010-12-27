# EFL port specific macros and definitions

FIND_PROGRAM(EDJE_CC_EXECUTABLE edje_cc)
IF (NOT EDJE_CC_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Missing edje_cc")
ENDIF ()
