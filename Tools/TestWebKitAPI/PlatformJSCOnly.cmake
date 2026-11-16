set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")

list(APPEND TestWTF_SOURCES generic/main.cpp)

if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
    list(APPEND TestWTF_SOURCES
        Tests/WTF/glib/FilePathWatcher.cpp
    )
    list(APPEND TestWTF_LIBRARIES
        GLib::Gio
    )
endif ()
