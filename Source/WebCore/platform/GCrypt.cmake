list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "platform/SourcesGCrypt.txt"
)

list(APPEND WebCore_LIBRARIES
    LibGcrypt::LibGcrypt
)

list(APPEND WebCore_PRIVATE_LIBRARIES
    Tasn1::Tasn1
)
