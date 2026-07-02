PROJECT(dbgmod)
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

SET (YASM_POSSIBLE_PATHS
    "$ENV{ProgramFiles}/Yasm/Bin"
    /usr/bin
    /usr/local/bin
    )

FIND_PROGRAM(YASM_PATH yasm
    PATHS ${YASM_POSSIBLE_PATHS}
    )

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

ADD_LIBRARY(dbgmod MODULE
    init_plugin.c
    dbg-objfmt.c
    )
TARGET_LINK_LIBRARIES(dbgmod ${YASM_LIBRARY})
