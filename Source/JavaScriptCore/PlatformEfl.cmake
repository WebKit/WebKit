LIST(APPEND JavaScriptCore_SOURCES
    jit/ExecutableAllocatorFixedVMPool.cpp
    jit/ExecutableAllocator.cpp
)

LIST(APPEND JavaScriptCore_LIBRARIES
    ${ICU_I18N_LIBRARIES}
)

IF (ENABLE_GLIB_SUPPORT)
  LIST(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/wtf/gobject
  )
ENDIF ()

LIST(APPEND JavaScriptCore_LINK_FLAGS
    ${ECORE_LDFLAGS}
)

LIST(APPEND JavaScriptCore_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/dfg
)

LIST(APPEND JavaScriptCore_SOURCES
    dfg/DFGAbstractState.cpp
    dfg/DFGAssemblyHelpers.cpp
    dfg/DFGByteCodeParser.cpp
    dfg/DFGCapabilities.cpp
    dfg/DFGCorrectableJumpPoint.cpp
    dfg/DFGDriver.cpp
    dfg/DFGGraph.cpp
    dfg/DFGJITCodeGenerator.cpp
    dfg/DFGJITCodeGenerator64.cpp
    dfg/DFGJITCodeGenerator32_64.cpp
    dfg/DFGJITCompiler.cpp
    dfg/DFGOperations.cpp
    dfg/DFGOSREntry.cpp
    dfg/DFGOSRExit.cpp
    dfg/DFGOSRExitCompiler.cpp
    dfg/DFGOSRExitCompiler64.cpp
    dfg/DFGOSRExitCompiler32_64.cpp
    dfg/DFGPropagator.cpp
    dfg/DFGRepatch.cpp
    dfg/DFGSpeculativeJIT.cpp
    dfg/DFGSpeculativeJIT64.cpp
    dfg/DFGSpeculativeJIT32_64.cpp
    dfg/DFGThunks.cpp
)
