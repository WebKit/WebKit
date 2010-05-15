SET(WTF_SOURCES
    Assertions.cpp
    ByteArray.cpp
    CurrentTime.cpp
    FastMalloc.cpp
    HashTable.cpp
    MainThread.cpp
    RandomNumber.cpp
    RefCountedLeakCounter.cpp
    StringExtras.cpp
    Threading.cpp
    TypeTraits.cpp
    WTFThreadData.cpp
    dtoa.cpp

    text/AtomicString.cpp
    text/CString.cpp
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

ADD_TARGET_PROPERTIES(${WTF_LIBRARY_NAME} LINK_FLAGS ${WTF_LINK_FLAGS})
