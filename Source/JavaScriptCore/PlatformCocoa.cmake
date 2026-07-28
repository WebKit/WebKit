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


# iOS-family framework packaging (identity, versioning, Info.plist, and the
# private headers / module maps / sandbox profile the iOS framework ships).
if (WEBKIT_SDK_IS_IOS_FAMILY)
    set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.JavaScriptCore)
    set_target_properties(JavaScriptCore PROPERTIES
        INSTALL_NAME_DIR "${JavaScriptCore_INSTALL_NAME_DIR}"
    )
    target_link_options(JavaScriptCore PRIVATE
        -compatibility_version 1.0.0
        -current_version ${WEBKIT_MAC_VERSION}
    )

    if (WTF_LIBRARY_TYPE STREQUAL "STATIC")
        target_link_options(JavaScriptCore PRIVATE
            "SHELL:-Wl,-force_load $<TARGET_FILE:WTF>"
        )
    endif ()

    # BrowserEngineCore provides the inline-JIT-permissions API (be_memory_*)
    # that threadSelfRestrict uses; weak-linked (iOS 17.4+).
    target_link_options(JavaScriptCore PRIVATE -weak_framework BrowserEngineCore)

    target_compile_definitions(JavaScriptCore PRIVATE PAS_BMALLOC_HIDDEN=1)
    target_compile_options(JavaScriptCore PRIVATE
        "$<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fvisibility=hidden>"
    )

    set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
    set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
    set(PRODUCT_NAME "JavaScriptCore")
    set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.JavaScriptCore")
    configure_file(${JAVASCRIPTCORE_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/JavaScriptCore-Info.plist)
    set(JavaScriptCore_POST_BUILD_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/JavaScriptCore-Info.plist
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/Info.plist
    )

    # arm64e: MacroAssembler.h conditionally includes this via CPU(ARM64E).
    list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
        assembler/MacroAssemblerARM64E.h
        heap/GCSegmentedArrayInlines.h
    )

    # Build-system scripts that ship as private framework headers.
    list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
        Scripts/UpdateContents.py
        Scripts/cssmin.py
        Scripts/generate-combined-inspector-json.py
        Scripts/generate-js-builtins.py
        Scripts/inline-and-minify-stylesheets-and-scripts.py
        Scripts/jsmin.py
        Scripts/lazywriter.py
        Scripts/make-js-file-arrays.py
        Scripts/xxd.pl

        Scripts/wkbuiltins/builtins_generate_combined_header.py
        Scripts/wkbuiltins/builtins_generate_combined_implementation.py
        Scripts/wkbuiltins/builtins_generate_internals_wrapper_header.py
        Scripts/wkbuiltins/builtins_generate_internals_wrapper_implementation.py
        Scripts/wkbuiltins/builtins_generate_separate_header.py
        Scripts/wkbuiltins/builtins_generate_separate_implementation.py
        Scripts/wkbuiltins/builtins_generate_wrapper_header.py
        Scripts/wkbuiltins/builtins_generate_wrapper_implementation.py
        Scripts/wkbuiltins/builtins_generator.py
        Scripts/wkbuiltins/builtins_model.py
        Scripts/wkbuiltins/builtins_templates.py
        Scripts/wkbuiltins/wkbuiltins.py

        inspector/scripts/generate-inspector-protocol-bindings.py

        inspector/scripts/codegen/cpp_generator.py
        inspector/scripts/codegen/cpp_generator_templates.py
        inspector/scripts/codegen/generate_cpp_alternate_backend_dispatcher_header.py
        inspector/scripts/codegen/generate_cpp_backend_dispatcher_header.py
        inspector/scripts/codegen/generate_cpp_backend_dispatcher_implementation.py
        inspector/scripts/codegen/generate_cpp_frontend_dispatcher_header.py
        inspector/scripts/codegen/generate_cpp_frontend_dispatcher_implementation.py
        inspector/scripts/codegen/generate_cpp_protocol_types_header.py
        inspector/scripts/codegen/generate_cpp_protocol_types_implementation.py
        inspector/scripts/codegen/generate_js_backend_commands.py
        inspector/scripts/codegen/generate_objc_backend_dispatcher_header.py
        inspector/scripts/codegen/generate_objc_backend_dispatcher_implementation.py
        inspector/scripts/codegen/generate_objc_configuration_header.py
        inspector/scripts/codegen/generate_objc_configuration_implementation.py
        inspector/scripts/codegen/generate_objc_frontend_dispatcher_implementation.py
        inspector/scripts/codegen/generate_objc_header.py
        inspector/scripts/codegen/generate_objc_internal_header.py
        inspector/scripts/codegen/generate_objc_protocol_type_conversions_header.py
        inspector/scripts/codegen/generate_objc_protocol_type_conversions_implementation.py
        inspector/scripts/codegen/generate_objc_protocol_types_implementation.py
        inspector/scripts/codegen/generator.py
        inspector/scripts/codegen/generator_templates.py
        inspector/scripts/codegen/models.py
        inspector/scripts/codegen/objc_generator.py
        inspector/scripts/codegen/objc_generator_templates.py
    )

    make_directory("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework")
    configure_file(${JAVASCRIPTCORE_DIR}/framework.sb ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/framework.sb COPYONLY)
    configure_file(${JAVASCRIPTCORE_DIR}/JavaScriptCore.modulemap ${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.modulemap COPYONLY)
    configure_file("${JAVASCRIPTCORE_DIR}/JavaScriptCore_Private.modulemap" ${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.private.modulemap COPYONLY)
endif ()

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSCallbackFunction.h
    API/JSContext.h
    API/JSContextPrivate.h
    API/JSContextRefPrivate.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSStringRefCF.h
    API/JSValue.h
    API/JSValuePrivate.h
    API/JSVirtualMachine.h
    API/JavaScriptCore.h
)

# Private headers the JavaScriptCore_Private module map needs to parse.
list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    API/MARReportCrashPrivate.h
    API/PASReportCrashPrivate.h
    API/WorkAround173516139.h

    assembler/MacroAssemblerPrinter.h

    debugger/DebuggerEvalEnabler.h

    disassembler/Disassembler.h

    heap/CodeBlockSet.h
    heap/ConservativeRoots.h
    heap/GCIncomingRefCountedSetInlines.h
    heap/HeapSnapshot.h
    heap/JITStubRoutineSet.h
    heap/VerifierSlotVisitorScope.h
    heap/WriteBarrierSupport.h

    inspector/augmentable/AlternateDispatchableAgent.h
    inspector/augmentable/AugmentableInspectorController.h

    jit/BinarySwitch.h
    jit/ExecutableAllocationFuzz.h
    jit/GdbJIT.h
    jit/JITExceptions.h
    jit/JSInterfaceJIT.h

    parser/ModuleScopeData.h

    runtime/PinballHandlerContext.h

    tools/JSDollarVM.h

    yarr/YarrJITRegisters.h
)

# Stage the module maps into the framework bundle so the Swift Clang importer
# finds JavaScriptCore / JavaScriptCore_Private as modules via -F. Otherwise
# <JavaScriptCore/*.h> is parsed textually and collides with other importers.
set(_jsc_modules_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/${WEBKIT_FRAMEWORK_VERSION_PATH}Modules")
add_custom_command(
    OUTPUT "${_jsc_modules_dir}/module.modulemap"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_jsc_modules_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${JAVASCRIPTCORE_DIR}/JavaScriptCore.modulemap" "${_jsc_modules_dir}/module.modulemap"
    MAIN_DEPENDENCY "${JAVASCRIPTCORE_DIR}/JavaScriptCore.modulemap"
    VERBATIM)
add_custom_command(
    OUTPUT "${_jsc_modules_dir}/module.private.modulemap"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_jsc_modules_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${JAVASCRIPTCORE_DIR}/JavaScriptCore_Private.modulemap" "${_jsc_modules_dir}/module.private.modulemap"
    MAIN_DEPENDENCY "${JAVASCRIPTCORE_DIR}/JavaScriptCore_Private.modulemap"
    VERBATIM)
add_custom_target(JavaScriptCore_CopyModules ALL DEPENDS
    "${_jsc_modules_dir}/module.modulemap"
    "${_jsc_modules_dir}/module.private.modulemap")
add_dependencies(JavaScriptCore JavaScriptCore_CopyModules)
