LIST(APPEND JavaScriptCore_SOURCES
    jit/ExecutableAllocatorFixedVMPool.cpp
    jit/ExecutableAllocator.cpp
)

LIST(APPEND JavaScriptCore_LIBRARIES
    ${ICU_I18N_LIBRARIES}
)

LIST(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

IF (ENABLE_GLIB_SUPPORT)
  LIST(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
  )
ENDIF ()

LIST(APPEND JavaScriptCore_LINK_FLAGS
    ${ECORE_LDFLAGS}
)
