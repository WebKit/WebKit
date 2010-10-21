SET(WTF_SOURCES
    Assertions.cpp
    ByteArray.cpp
    CurrentTime.cpp
    DecimalNumber.cpp
    FastMalloc.cpp
    HashTable.cpp
    MainThread.cpp
    MD5.cpp
    RandomNumber.cpp
    RefCountedLeakCounter.cpp
    StringExtras.cpp
    Threading.cpp
    TypeTraits.cpp
    WTFThreadData.cpp
    dtoa.cpp

    text/AtomicString.cpp
    text/CString.cpp
    text/StringBuilder.cpp
    text/StringImpl.cpp
    text/StringStatics.cpp
    text/WTFString.cpp

    unicode/UTF8.cpp
)

SET(WTF_LIBRARIES
)

SET(WTF_PORT_FLAGS )
INCLUDE_IF_EXISTS(${JAVASCRIPTCORE_DIR}/wtf/CMakeLists${PORT}.txt)

LIST(APPEND WTF_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
)

WEBKIT_WRAP_SOURCELIST(${WTF_SOURCES})
INCLUDE_DIRECTORIES(${WTF_INCLUDE_DIRECTORIES})
ADD_DEFINITIONS(-DBUILDING_WTF)
ADD_LIBRARY(${WTF_LIBRARY_NAME} ${WTF_LIBRARY_TYPE} ${WTF_SOURCES})
TARGET_LINK_LIBRARIES(${WTF_LIBRARY_NAME} ${WTF_LIBRARIES})

IF (WTF_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(${WTF_LIBRARY_NAME} LINK_FLAGS "${WTF_LINK_FLAGS}")
ENDIF ()

IF (SHARED_CORE)
    SET_TARGET_PROPERTIES(${WTF_LIBRARY_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    INSTALL(TARGETS ${WTF_LIBRARY_NAME} DESTINATION lib)
ENDIF ()
