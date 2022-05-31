if (true)
    if (USE_GLIB)
        include(inspector/remote/GLib.cmake)
    elseif (USE_INSPECTOR_SOCKET_SERVER)
        include(inspector/remote/Socket.cmake)
    elseif (APPLE)
        include(inspector/remote/Cocoa.cmake)
    endif ()
endif ()

if (USE_GLIB)
    list(APPEND JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND JavaScriptCore_LIBRARIES
        ${GLIB_LIBRARIES}
    )
endif ()
