LIST(INSERT JavaScriptCore_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)

LIST(REMOVE_ITEM JavaScriptCore_SOURCES
    runtime/GCActivityCallback.cpp
)

LIST(APPEND JavaScriptCore_SOURCES
    runtime/GCActivityCallbackBlackBerry.cpp
    runtime/MemoryStatistics.cpp
)

INSTALL(FILES "wtf/Forward.h" DESTINATION usr/include/browser/webkit/wtf)
