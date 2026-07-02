add_definitions(-D__STDC_WANT_LIB_EXT1__)

target_compile_options(JavaScriptCore PRIVATE
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fno-threadsafe-statics>"
)

find_library(SECURITY_LIBRARY Security)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(CORETEXT_LIBRARY CoreText)
list(APPEND JavaScriptCore_LIBRARIES
    ${SECURITY_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
    ${CORETEXT_LIBRARY}
)

target_link_options(JavaScriptCore PRIVATE
    -Wl,-unexported_symbols_list,${JAVASCRIPTCORE_DIR}/unexported-libc++.txt
)

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"

    "inspector/remote/SourcesCocoa.txt"
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    ${JAVASCRIPTCORE_DIR}/inspector/cocoa
    ${JAVASCRIPTCORE_DIR}/inspector/remote/cocoa
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    inspector/remote/RemoteInspectorConstants.h

    inspector/remote/cocoa/RemoteInspectorXPCConnection.h
)

# Headers Xcode marks Private but cmake omits from JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS.
list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcode.h

    bytecode/DirectEvalCodeCacheInlines.h

    runtime/FractionToDouble.h

    wasm/WasmTypeSectionState.h

    wasm/js/JSWebAssemblyStreamingContextInlines.h
)

list(REMOVE_ITEM JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    API/glib/JSAPIWrapperGlobalObject.h
    API/glib/JSCAutocleanups.h
    API/glib/JSCCallbackFunction.h
    API/glib/JSCClassPrivate.h
    API/glib/JSCContextInternal.h
    API/glib/JSCContextPrivate.h
    API/glib/JSCExceptionPrivate.h
    API/glib/JSCGLibWrapperObject.h
    API/glib/JSCOptions.h
    API/glib/JSCValuePrivate.h
    API/glib/JSCVirtualMachinePrivate.h
    API/glib/JSCWrapperMap.h
)

list(REMOVE_ITEM JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    API/JSBase.h
    API/JSCallbackConstructor.h
    API/JSCallbackObject.h
    API/JSContext.h
    API/JSContextRef.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSObjectRef.h
    API/JSStringRef.h
    API/JSStringRefCF.h
    API/JSTypedArray.h
    API/JSValue.h
    API/JSValueRef.h
    API/JSVirtualMachine.h
    API/JavaScript.h
    API/JavaScriptCore.h
)

list(REMOVE_ITEM JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    API/APICallbackFunction.h

    b3/testb3.h

    bytecode/BytecodeBasicBlock.h
    bytecode/ChainedWatchpoint.h
    bytecode/CodeBlockInlines.h
    bytecode/InlineAccess.h
    bytecode/MethodOfGettingAValueProfile.h
    bytecode/TrackedReferences.h

    dfg/DFGCPSRethreadingPhase.h
    dfg/DFGCSEPhase.h
    dfg/DFGInPlaceAbstractState.h
    dfg/DFGLiveCatchVariablePreservationPhase.h
    dfg/DFGRegisteredStructure.h
    dfg/DFGRegisteredStructureSet.h

    ftl/FTLAbbreviatedTypes.h
    ftl/FTLCommonValues.h
    ftl/FTLExitArgumentForOperand.h
    ftl/FTLExitValue.h
    ftl/FTLFormattedValue.h
    ftl/FTLOSREntry.h
    ftl/FTLStackmapArgumentList.h
    ftl/FTLThunks.h
    ftl/FTLValueFromBlock.h
    ftl/FTLValueRange.h

    jit/JITThunks.h
    jit/SIMDShuffle.h
    jit/SpillRegistersMode.h

    llint/InPlaceInterpreter.h
    llint/LLIntCLoop.h
    llint/LLIntOfflineAsmConfig.h
    llint/LLIntPCRanges.h

    lol/LOLJIT.h
    lol/LOLRegisterAllocator.h

    profiler/ProfilerDumper.h

    runtime/ArrayIteratorPrototype.h
    runtime/BigInteger.h
    runtime/IndexingTypeInlines.h
    runtime/JSSetIteratorInlines.h
    runtime/JSSourceCodeInlines.h
    runtime/JSStringIteratorInlines.h
    runtime/JSStringJoiner.h
    runtime/MachineContext.h
    runtime/ModuleGraphLoadingState.h
    runtime/ModuleLoaderPayload.h
    runtime/ModuleLoadingContext.h
    runtime/ModuleRegistryEntry.h
    runtime/RegExpInlines.h
    runtime/RegExpMatchesArray.h
    runtime/TemporalNow.h

    wasm/WasmParser.h
    wasm/WasmPlan.h

    wasm/debugger/WasmBreakpointManager.h
    wasm/debugger/WasmExecutionHandler.h
    wasm/debugger/WasmMemoryHandler.h
    wasm/debugger/WasmModuleManager.h
    wasm/debugger/WasmQueryHandler.h

    wasm/js/WebAssemblyPromising.h
    wasm/js/WebAssemblySuspending.h
    wasm/js/WebAssemblySuspendingConstructor.h
    wasm/js/WebAssemblySuspendingPrototype.h
)

if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/AugmentableInspectorControllerClient.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/AugmentableInspectorControllerClient.h "#include \"inspector/augmentable/AugmentableInspectorControllerClient.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendRouter.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendRouter.h "#include \"inspector/InspectorFrontendRouter.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatcher.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatcher.h "#include \"inspector/InspectorBackendDispatcher.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatchers.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorBackendDispatchers.h "#include \"inspector/InspectorBackendDispatchers.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendDispatchers.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorFrontendDispatchers.h "#include \"inspector/InspectorFrontendDispatchers.h\"")
endif ()
if (NOT EXISTS ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorProtocolObjects.h)
    file(WRITE ${JavaScriptCore_DERIVED_SOURCES_DIR}/InspectorProtocolObjects.h "#include \"inspector/InspectorProtocolObjects.h\"")
endif ()
