list(APPEND jsc_LIBRARIES
    ${GLIB_LIBRARIES}
)

# FIXME: Remove when turning on hidden visibility https://bugs.webkit.org/show_bug.cgi?id=181916
list(APPEND jsc_LIBRARIES WTF)
list(APPEND testapi_LIBRARIES  WTF)
list(APPEND testmasm_LIBRARIES WTF)
list(APPEND testb3_LIBRARIES WTF)
