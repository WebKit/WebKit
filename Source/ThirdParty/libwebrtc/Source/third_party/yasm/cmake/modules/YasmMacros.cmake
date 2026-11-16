# Portions based on kdelibs KDE4Macros.cmake:
#
# Copyright (c) 2006, 2007, Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2006, 2007, Laurent Montel, <montel@kde.org>
# Copyright (c) 2007 Matthias Kretz <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
#
# Changes for Yasm Copyright (c) 2007 Peter Johnson

# add a unit test, which is executed when running make test
# it will be built with RPATH pointing to the build dir
# The targets are always created, but only built for the "all"
# target if the option YASM_BUILD_TESTS is enabled. Otherwise the rules for
# the target are created but not built by default. You can build them by
# manually building the target.
# The name of the target can be specified using TESTNAME <testname>, if it is
# not given the macro will default to the <name>
macro (YASM_ADD_UNIT_TEST _test_NAME)
    set(_srcList ${ARGN})
    set(_targetName ${_test_NAME})
    if( ${ARGV1} STREQUAL "TESTNAME" )
        set(_targetName ${ARGV2})
        LIST(REMOVE_AT _srcList 0 1)
    endif( ${ARGV1} STREQUAL "TESTNAME" )
    yasm_add_test_executable( ${_test_NAME} ${_srcList} )
    add_test( ${_targetName} ${EXECUTABLE_OUTPUT_PATH}/${_test_NAME} )
endmacro (YASM_ADD_UNIT_TEST)

# add an test executable
# it will be built with RPATH pointing to the build dir
# The targets are always created, but only built for the "all"
# target if the option YASM_BUILD_TESTS is enabled. Otherwise the rules for
# the target are created but not built by default. You can build them by
# manually building the target.
macro (YASM_ADD_TEST_EXECUTABLE _target_NAME)

   set(_add_executable_param)

   if (NOT YASM_BUILD_TESTS)
      set(_add_executable_param EXCLUDE_FROM_ALL)
   endif (NOT YASM_BUILD_TESTS)

   set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

   set(_SRCS ${ARGN})
   add_executable(${_target_NAME} ${_add_executable_param} ${_SRCS})

   set_target_properties(${_target_NAME} PROPERTIES
                         SKIP_BUILD_RPATH FALSE
                         BUILD_WITH_INSTALL_RPATH FALSE)

endmacro (YASM_ADD_TEST_EXECUTABLE)

macro (YASM_ADD_MODULE _module_NAME)
    list(APPEND YASM_MODULES_SRC ${ARGN})
    list(APPEND YASM_MODULES ${_module_NAME})
endmacro (YASM_ADD_MODULE)

macro (YASM_GENPERF _in_NAME _out_NAME)
    get_target_property(_tmp_GENPERF_EXE genperf LOCATION)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND ${_tmp_GENPERF_EXE} ${_in_NAME} ${_out_NAME}
        DEPENDS ${_tmp_GENPERF_EXE}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_GENPERF)

macro (YASM_RE2C _in_NAME _out_NAME)
    get_target_property(_tmp_RE2C_EXE re2c LOCATION)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND ${_tmp_RE2C_EXE} ${ARGN} -o ${_out_NAME} ${_in_NAME}
        DEPENDS ${_tmp_RE2C_EXE}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_RE2C)

macro (YASM_GENMACRO _in_NAME _out_NAME _var_NAME)
    get_target_property(_tmp_GENMACRO_EXE genmacro LOCATION)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND ${_tmp_GENMACRO_EXE} ${_out_NAME} ${_var_NAME} ${_in_NAME}
        DEPENDS ${_tmp_GENMACRO_EXE}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_GENMACRO)

