if (ENABLE_WEBKIT)
    add_subdirectory(${WEBKIT_DIR}/gtk/tests)
endif ()

if (ENABLE_WEBKIT2)
    add_subdirectory(${WEBKIT2_DIR}/UIProcess/API/gtk/tests)
endif ()
