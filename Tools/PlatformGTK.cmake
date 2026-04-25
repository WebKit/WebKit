if (DEVELOPER_MODE)
    add_subdirectory(flatpak)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(TestWebKitAPI/glib)
endif ()

if (ENABLE_MINIBROWSER)
  add_subdirectory(MiniBrowser/gtk)
endif ()
