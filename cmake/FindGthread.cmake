# Find include and libraries for GTHREAD library
# GTHREAD_INCLUDE     Directories to include to use GTHREAD
# GTHREAD_INCLUDE-I   Directories to include to use GTHREAD (with -I)
# GTHREAD_LIBRARIES   Libraries to link against to use GTHREAD
# GTHREAD_FOUND       GTHREAD was found

IF (UNIX)
    INCLUDE (UsePkgConfig)
    PKGCONFIG (gthread-2.0 GTHREAD_include_dir GTHREAD_link_dir GTHREAD_libraries GTHREAD_include)
    IF (GTHREAD_include AND GTHREAD_libraries)
        SET (GTHREAD_FOUND TRUE)
        EXEC_PROGRAM ("echo"
            ARGS "${GTHREAD_include} | sed 's/[[:blank:]]*-I/;/g'"
            OUTPUT_VARIABLE GTHREAD_INCLUDE
        )
        SET (GTHREAD_INCLUDE-I ${GTHREAD_include})
        SET (GTHREAD_LIBRARIES ${GTHREAD_libraries})
    ELSE (GTHREAD_include AND GTHREAD_libraries)
        SET (GTHREAD_FOUND FALSE)
    ENDIF (GTHREAD_include AND GTHREAD_libraries)
ENDIF (UNIX)
