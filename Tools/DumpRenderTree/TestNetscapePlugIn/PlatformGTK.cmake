if (MOZ_X11)
    list(APPEND TestNetscapePlugIn_SYSTEM_INCLUDE_DIRECTORIES
        ${X11_INCLUDE_DIR}
    )
    list(APPEND TestNetscapePlugIn_LIBRARIES
        ${X11_LIBRARIES}
    )
    list(APPEND TestNetscapePlugIn_DEFINITIONS MOZ_X11)
endif ()

if (XP_UNIX)
    list(APPEND TestNetscapePlugIn_SOURCES
        Tests/unix/CallInvalidateRectWithNullNPPArgument.cpp
    )
    list(APPEND TestNetscapePlugIn_DEFINITIONS XP_UNIX)
endif ()

set_target_properties(TestNetscapePlugIn PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins)
