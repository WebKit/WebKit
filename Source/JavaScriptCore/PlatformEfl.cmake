list(APPEND JavaScriptCore_SOURCES
    jit/ExecutableAllocatorFixedVMPool.cpp
    jit/ExecutableAllocator.cpp

    runtime/MemoryStatistics.cpp
)

list(APPEND JavaScriptCore_LIBRARIES
    ${ICU_I18N_LIBRARIES}
)

list(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
    ${ICU_INCLUDE_DIRS}
)

