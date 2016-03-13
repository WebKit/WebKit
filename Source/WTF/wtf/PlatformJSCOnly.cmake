if (LOWERCASE_EVENTLOOP_TYPE STREQUAL "glib")
    list(APPEND WTF_SOURCES
        glib/GRefPtr.cpp
        glib/MainThreadGLib.cpp
        glib/RunLoopGLib.cpp
        glib/WorkQueueGLib.cpp
    )
    list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND WTF_LIBRARIES
        ${GLIB_GIO_LIBRARIES}
        ${GLIB_GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
    )
else ()
    list(APPEND WTF_SOURCES
        none/MainThreadNone.cpp
        none/RunLoopNone.cpp
        none/WorkQueueNone.cpp
    )
endif ()

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
)
