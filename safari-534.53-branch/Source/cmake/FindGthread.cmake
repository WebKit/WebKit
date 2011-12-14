# Find include and libraries for Gthread library

INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES (Gthread REQUIRED gthread-2.0>=2.20.0)
