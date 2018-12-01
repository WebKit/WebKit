list(APPEND WTF_PUBLIC_HEADERS
    glib/GLibUtilities.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/GUniquePtrSoup.h
    glib/RunLoopSourcePriority.h
    glib/WTFGType.h

    linux/CurrentProcessMemoryStatus.h
)

list(APPEND WTF_SOURCES
    UniStdExtras.cpp

    generic/MainThreadGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/GLibUtilities.cpp
    glib/GRefPtr.cpp
    glib/RunLoopGLib.cpp
    glib/URLSoup.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/MemoryFootprintLinux.cpp
    linux/MemoryPressureHandlerLinux.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
    unix/LanguageUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)
