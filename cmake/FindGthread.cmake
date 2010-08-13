# Find include and libraries for GTHREAD library
# GTHREAD_INCLUDE     Directories to include to use GTHREAD
# GTHREAD_INCLUDE-I   Directories to include to use GTHREAD (with -I)
# GTHREAD_LIBRARIES   Libraries to link against to use GTHREAD
# GTHREAD_FOUND       GTHREAD was found

INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES (Gthread REQUIRED gthread-2.0>=2.20.0)
