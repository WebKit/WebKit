list(APPEND WTF_SOURCES
    efl/DispatchQueueEfl.cpp
    efl/MainThreadEfl.cpp
    efl/RunLoopEfl.cpp
    efl/WorkQueueEfl.cpp

    glib/GMainLoopSource.cpp
    glib/GRefPtr.cpp
    glib/GThreadSafeMainLoopSource.cpp
)

list(APPEND WTF_LIBRARIES
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_IMF_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EVAS_LIBRARIES}
    pthread
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    "${WTF_DIR}/wtf/efl"
)

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_IMF_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
)
