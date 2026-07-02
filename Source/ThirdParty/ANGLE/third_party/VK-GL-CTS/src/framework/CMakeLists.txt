# Test framework cmake file

# C integration library
add_subdirectory(qphelper)

# Common test util (tcutil).
add_subdirectory(common)

# Common OpenGL utilities
add_subdirectory(opengl)

# EGL utilities
add_subdirectory(egl)

# XE xml util
add_subdirectory(xexml)

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/randomshaders)
	add_subdirectory(randomshaders)
endif ()

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/referencerenderer)
	add_subdirectory(referencerenderer)
endif ()

# Platform library
add_subdirectory(platform)
