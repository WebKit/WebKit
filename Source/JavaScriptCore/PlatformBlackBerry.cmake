LIST(INSERT JavaScriptCore_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)

LIST(APPEND JavaScriptCore_SOURCES
    DisassemblerARM.cpp
)

INSTALL(FILES "wtf/Forward.h" DESTINATION usr/include/browser/webkit/wtf)
