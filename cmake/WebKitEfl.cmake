# EFL port specific macros and definitions

FIND_PROGRAM(EDJE_CC_EXECUTABLE edje_cc)
IF (NOT EDJE_CC_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Missing edje_cc")
ENDIF ()

# - Generate Edje compiled from the given source
# GENERATE_EDJ(source binary edje_cc_options)
#
# This runs edje_cc -v ${edje_cc_options} ${source} ${binary}
MACRO(GENERATE_EDJ _edc _edj _edje_cc_opts)
  ADD_CUSTOM_COMMAND(
    OUTPUT  ${_edj}
    COMMAND ${EDJE_CC_EXECUTABLE} -v ${_edje_cc_opts} ${_edc} ${_edj}
    VERBATIM)
ENDMACRO()
