# - Try to find LibXslt
# Once done this will define
#
#  LIBXSLT_FOUND - system has LibXslt
#  LIBXSLT_INCLUDE_DIR - the LibXslt include directory
#  LIBXSLT_LIBRARIES - Link these to LibXslt
#  LIBXSLT_DEFINITIONS - Compiler switches required for using LibXslt
#  LIBXSLT_XSLTPROC_EXECUTABLE - path to the xsltproc tool

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


IF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)
   # in cache already
   SET(LibXslt_FIND_QUIETLY TRUE)
ENDIF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)

IF (NOT WIN32)
   # use pkg-config to get the directories and then use these values
   # in the FIND_PATH() and FIND_LIBRARY() calls
   find_package(PkgConfig)
   pkg_check_modules(PC_XSLT QUIET libxslt)
   SET(LIBXSLT_DEFINITIONS ${PC_XSLT_CFLAGS_OTHER})
ENDIF (NOT WIN32)

FIND_PATH(LIBXSLT_INCLUDE_DIR libxslt/xslt.h
    PATHS
    ${PC_XSLT_INCLUDEDIR}
    ${PC_XSLT_INCLUDE_DIRS}
  )

FIND_LIBRARY(LIBXSLT_LIBRARIES NAMES xslt libxslt
    PATHS
    ${PC_XSLT_LIBDIR}
    ${PC_XSLT_LIBRARY_DIRS}
  )

FIND_LIBRARY(LIBEXSLT_LIBRARIES NAMES exslt libexslt
    PATHS
    ${PC_XSLT_LIBDIR}
    ${PC_XSLT_LIBRARY_DIRS}
  )

IF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)
   SET(LIBXSLT_FOUND TRUE)
ELSE (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)
   SET(LIBXSLT_FOUND FALSE)
ENDIF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)

FIND_PROGRAM(LIBXSLT_XSLTPROC_EXECUTABLE xsltproc)
# For compatibility with FindLibXslt.cmake from KDE 4.[01].x
SET(XSLTPROC_EXECUTABLE ${LIBXSLT_XSLTPROC_EXECUTABLE})

IF (LIBXSLT_FOUND)
   IF (NOT LibXslt_FIND_QUIETLY)
      MESSAGE(STATUS "Found LibXslt: ${LIBXSLT_LIBRARIES}")
   ENDIF (NOT LibXslt_FIND_QUIETLY)
ELSE (LIBXSLT_FOUND)
   IF (LibXslt_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could NOT find LibXslt")
   ENDIF (LibXslt_FIND_REQUIRED)
ENDIF (LIBXSLT_FOUND)

MARK_AS_ADVANCED(LIBXSLT_INCLUDE_DIR  LIBXSLT_LIBRARIES  LIBEXSLT_LIBRARIES  LIBXSLT_XSLTPROC_EXECUTABLE)

