list(APPEND WTF_SOURCES
    OSAllocatorPosix.cpp
    OSAllocatorWin.cpp
    ThreadIdentifierDataPthreads.cpp
    ThreadingPthreads.cpp
    ThreadingWin.cpp

    gobject/GlibUtilities.cpp
    gobject/GOwnPtr.cpp
    gobject/GRefPtr.cpp

    gtk/MainThreadGtk.cpp

    unicode/icu/CollatorICU.cpp

    win/OwnPtrWin.cpp
)

list(APPEND WTF_LIBRARIES
    pthread
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${ICU_LIBRARIES}
    ${ICU_I18N_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
    ${JAVASCRIPTCORE_DIR}/wtf/unicode
)
