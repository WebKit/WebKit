LIST(APPEND WTF_SOURCES
    efl/MainThreadEfl.cpp
    efl/OwnPtrEfl.cpp
    gobject/GOwnPtr.cpp
    gobject/GRefPtr.cpp

    OSAllocatorPosix.cpp
    ThreadIdentifierDataPthreads.cpp
    ThreadingPthreads.cpp

    unicode/icu/CollatorICU.cpp
)

LIST(APPEND WTF_LIBRARIES
    pthread
    ${Glib_LIBRARIES}
    ${ICU_LIBRARIES}
    ${ICU_I18N_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

LIST(APPEND WTF_LINK_FLAGS
    ${ECORE_LDFLAGS}
    ${ECORE_EVAS_LDFLAGS}
    ${EVAS_LDFLAGS}
)

LIST(APPEND WTF_INCLUDE_DIRECTORIES
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${Glib_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
    ${JAVASCRIPTCORE_DIR}/wtf/unicode/
)
