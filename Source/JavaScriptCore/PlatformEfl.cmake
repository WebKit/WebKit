list(APPEND JavaScriptCore_SOURCES
    jit/ExecutableAllocatorFixedVMPool.cpp
    jit/ExecutableAllocator.cpp

    runtime/MemoryStatistics.cpp
)

list(APPEND JavaScriptCore_LIBRARIES
    ${ICU_I18N_LIBRARIES}
)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

if (ENABLE_GLIB_SUPPORT)
    list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
         ${JAVASCRIPTCORE_DIR}/wtf/gobject
    )
endif ()
