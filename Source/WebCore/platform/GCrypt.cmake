if (ENABLE_WEB_CRYPTO)
    list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
        "platform/SourcesGCrypt.txt"
    )
endif ()

list(APPEND WebCore_LIBRARIES
    LibGcrypt::LibGcrypt
)
