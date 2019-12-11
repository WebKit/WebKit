# Allow building ANGLE on platforms that don't provide X11 headers.
list(APPEND ANGLE_DEFINITIONS USE_SYSTEM_EGL)
