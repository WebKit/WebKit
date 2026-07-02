include(PlatformCocoa.cmake)

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

set(JavaScriptCore_POST_BUILD_COMMAND
    ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/JavaScriptCore-Info.plist
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/Info.plist
    COMMAND codesign --force --sign - ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework
)
set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
set(SHORT_VERSION_STRING "${WEBKIT_MAC_VERSION}")
set(PRODUCT_NAME "JavaScriptCore")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.JavaScriptCore")
configure_file(${JAVASCRIPTCORE_DIR}/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/JavaScriptCore-Info.plist)

# Weak-linked: iOS 17.4+.
target_link_options(JavaScriptCore PRIVATE -weak_framework BrowserEngineCore)

target_compile_options(JavaScriptCore PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})

target_compile_definitions(JavaScriptCore PRIVATE PAS_BMALLOC_HIDDEN=1)

target_compile_options(JavaScriptCore PRIVATE
    "$<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fvisibility=hidden>"
)

list(APPEND JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSContext.h
    API/JSExport.h
    API/JSManagedValue.h
    API/JSStringRefCF.h
    API/JSValue.h
    API/JSVirtualMachine.h
    API/JavaScriptCore.h
)

list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    API/JSContextPrivate.h
    API/JSContextRefPrivate.h
    API/JSValuePrivate.h
)

# arm64e: MacroAssembler.h conditionally includes this via CPU(ARM64E).
list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    assembler/MacroAssemblerARM64E.h
)

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
    heap/GCSegmentedArrayInlines.h
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

configure_file(${JAVASCRIPTCORE_DIR}/JavaScriptCore.modulemap ${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.modulemap COPYONLY)
# FIXME: Private module map requires full PrivateHeaders install.
# https://bugs.webkit.org/show_bug.cgi?id=312083

make_directory("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework")
configure_file(${JAVASCRIPTCORE_DIR}/framework.sb ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/framework.sb COPYONLY)
set(_jsc_fw "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework")
if (NOT EXISTS "${_jsc_fw}/Headers")
    file(CREATE_LINK "${JavaScriptCore_FRAMEWORK_HEADERS_DIR}/JavaScriptCore"
                     "${_jsc_fw}/Headers" SYMBOLIC)
endif ()
if (NOT EXISTS "${_jsc_fw}/PrivateHeaders")
    file(CREATE_LINK "${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/JavaScriptCore"
                     "${_jsc_fw}/PrivateHeaders" SYMBOLIC)
endif ()
if (NOT EXISTS "${_jsc_fw}/Modules")
    file(CREATE_LINK "${CMAKE_BINARY_DIR}/JavaScriptCore/Modules"
                     "${_jsc_fw}/Modules" SYMBOLIC)
endif ()

configure_file("${JAVASCRIPTCORE_DIR}/JavaScriptCore_Private.modulemap"
               "${CMAKE_BINARY_DIR}/JavaScriptCore/Modules/module.private.modulemap" COPYONLY)
unset(_jsc_fw)
