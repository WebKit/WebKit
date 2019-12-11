PROJECT(x86mod)
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

SET(YASM_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../.."
    CACHE STRING "Location of Yasm source")

IF (NOT YASM_SOURCE_DIR)
    MESSAGE(FATAL_ERROR "YASM_SOURCE_DIR must be defined")
ENDIF (NOT YASM_SOURCE_DIR)

SET(SOURCE_DIR "${YASM_SOURCE_DIR}/modules/arch/x86")

SET (YASM_POSSIBLE_PATHS
    "$ENV{ProgramFiles}/Yasm/Bin"
    /usr/bin
    /usr/local/bin
    )

FIND_PROGRAM(YASM_PATH yasm
    PATHS ${YASM_POSSIBLE_PATHS}
    )

SET (GENPERF_POSSIBLE_PATHS
    ${YASM_SOURCE_DIR}/objdir/tools/genperf
    ${YASM_SOURCE_DIR}/objdir/tools/genperf/Debug
    ${YASM_SOURCE_DIR}/objdir/tools/genperf/Release
    )

FIND_PROGRAM(GENPERF_PATH genperf
    DOC "The path to the yasm genperf executable"
    PATHS ${GENPERF_POSSIBLE_PATHS}
    )

IF (NOT GENPERF_PATH)
    MESSAGE(FATAL_ERROR "Could not find genperf executable")
ENDIF (NOT GENPERF_PATH)

SET (YASM_POSSIBLE_INCLUDE_PATHS
    "${YASM_PATH}"
    "${YASM_PATH}/../include"
    "$ENV{ProgramFiles}/Yasm/Include"
    /usr/include
    /usr/local/include
    )

FIND_PATH(YASM_INCLUDE_PATH NAMES libyasm.h
    DOC "The path to the libyasm include files"
    PATHS ${YASM_POSSIBLE_INCLUDE_PATHS}
    )

IF (NOT YASM_INCLUDE_PATH)
    MESSAGE(FATAL_ERROR "Could not find yasm include files")
ENDIF (NOT YASM_INCLUDE_PATH)

INCLUDE_DIRECTORIES(${YASM_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${YASM_SOURCE_DIR})

SET (YASM_POSSIBLE_LIB_PATHS
    "${YASM_PATH}"
    "${YASM_PATH}/../lib"
    "${YASM_INCLUDE_PATH}/../lib"
    "$ENV{ProgramFiles}/Yasm/Lib"
    /usr/lib
    /usr/local/lib
    )

FIND_LIBRARY(YASM_LIBRARY
    NAMES yasm
    DOC "The path to the libyasm library"
    PATHS ${YASM_POSSIBLE_LIB_PATHS}
    )

IF (NOT YASM_LIBRARY)
    MESSAGE(FATAL_ERROR "Could not find yasm library")
ENDIF (NOT YASM_LIBRARY)

INCLUDE(FindPythonInterp)

IF (NOT PYTHON_EXECUTABLE)
    MESSAGE(FATAL_ERROR "Could not find Python executable")
ENDIF (NOT PYTHON_EXECUTABLE)

INCLUDE_DIRECTORIES(${SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/x86insns.c
        ${CMAKE_CURRENT_BINARY_DIR}/x86insn_gas.gperf
        ${CMAKE_CURRENT_BINARY_DIR}/x86insn_nasm.gperf
    COMMAND ${PYTHON_EXECUTABLE} ${SOURCE_DIR}/gen_x86_insn.py
           ${CMAKE_CURRENT_BINARY_DIR}/x86insns.c
           ${CMAKE_CURRENT_BINARY_DIR}/x86insn_gas.gperf
           ${CMAKE_CURRENT_BINARY_DIR}/x86insn_nasm.gperf
    MAIN_DEPENDENCY ${SOURCE_DIR}/gen_x86_insn.py
    )

macro (YASM_GENPERF _in_NAME _out_NAME)
    add_custom_command(
        OUTPUT ${_out_NAME}
        COMMAND ${GENPERF_PATH} ${_in_NAME} ${_out_NAME}
        MAIN_DEPENDENCY ${_in_NAME}
        )
endmacro (YASM_GENPERF)

YASM_GENPERF(
    ${SOURCE_DIR}/x86cpu.gperf
    ${CMAKE_CURRENT_BINARY_DIR}/x86cpu.c
    )

YASM_GENPERF(
    ${SOURCE_DIR}/x86regtmod.gperf
    ${CMAKE_CURRENT_BINARY_DIR}/x86regtmod.c
    )

YASM_GENPERF(
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_nasm.gperf
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_nasm.c
    )

YASM_GENPERF(
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_gas.gperf
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_gas.c
    )

SET(insn_DEPS
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_nasm.c
    ${CMAKE_CURRENT_BINARY_DIR}/x86insn_gas.c
    ${CMAKE_CURRENT_BINARY_DIR}/x86insns.c
    )

SET_SOURCE_FILES_PROPERTIES(${SOURCE_DIR}/x86id.c PROPERTIES
    OBJECT_DEPENDS "${insn_DEPS}"
    )

ADD_LIBRARY(x86mod MODULE
    init_plugin.c
    ${SOURCE_DIR}/x86arch.c
    ${SOURCE_DIR}/x86bc.c
    ${SOURCE_DIR}/x86expr.c
    ${SOURCE_DIR}/x86id.c
    x86cpu.c
    x86regtmod.c
    )
TARGET_LINK_LIBRARIES(x86mod ${YASM_LIBRARY})
