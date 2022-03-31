if (USE_WPE_BACKEND_PLAYSTATION AND (DEVELOPER_MODE OR ENABLE_MINIBROWSER))
    add_subdirectory(wpe/backends)
endif ()

if (ENABLE_MINIBROWSER)
    add_subdirectory(MiniBrowser/playstation)
endif ()
