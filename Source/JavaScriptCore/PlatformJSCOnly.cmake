if (ENABLE_REMOTE_INSPECTOR)
    if (USE_GLIB)
        include(inspector/remote/GLib.cmake)
    elseif (APPLE)
        include(inspector/remote/Cocoa.cmake)
    else ()
        include(inspector/remote/Socket.cmake)
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
