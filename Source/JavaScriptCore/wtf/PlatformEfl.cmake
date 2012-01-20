LIST(APPEND WTF_SOURCES
    efl/MainThreadEfl.cpp
    efl/OwnPtrEfl.cpp

    OSAllocatorPosix.cpp
    ThreadIdentifierDataPthreads.cpp
    ThreadingPthreads.cpp

    unicode/icu/CollatorICU.cpp
)

IF (ENABLE_GLIB_SUPPORT)
  LIST(APPEND WTF_SOURCES
    gobject/GOwnPtr.cpp
    gobject/GRefPtr.cpp
  )

  LIST(APPEND WTF_INCLUDE_DIRECTORIES
    ${Glib_INCLUDE_DIRS}
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
  )

  LIST(APPEND WTF_LIBRARIES
    ${Glib_LIBRARIES}
  )
ENDIF ()

LIST(APPEND WTF_LIBRARIES
    pthread
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
    ${ICU_INCLUDE_DIRS}
    ${JAVASCRIPTCORE_DIR}/wtf/unicode/
)
