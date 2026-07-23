include(PlatformCocoa.cmake)

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
# Mirrors the list in PlatformIOS.cmake.
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
set(_jsc_modules_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/JavaScriptCore.framework/Versions/A/Modules")
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
