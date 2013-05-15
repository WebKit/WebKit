list(APPEND WTF_SOURCES
    blackberry/MainThreadBlackBerry.cpp
)

list(INSERT WTF_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)
