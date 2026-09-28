if (ENABLE_WPE_LEGACY_API AND (ENABLE_API_TESTS OR ENABLE_LAYOUT_TESTS OR ENABLE_MINIBROWSER))
    add_subdirectory(wpe/backends)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI/glib)
endif ()

if (ENABLE_MINIBROWSER)
    add_subdirectory(MiniBrowser/wpe)
endif ()

