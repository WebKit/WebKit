cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)
set_property(DIRECTORY . PROPERTY FOLDER "JavaScriptCore")

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(JavaScriptCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/.."
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/b3"
    "${JAVASCRIPTCORE_DIR}/b3/air"
    "${JAVASCRIPTCORE_DIR}/bindings"
    "${JAVASCRIPTCORE_DIR}/builtins"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/disassembler/udis86"
    "${JAVASCRIPTCORE_DIR}/disassembler/ARM64"
    "${JAVASCRIPTCORE_DIR}/domjit"
    "${JAVASCRIPTCORE_DIR}/ftl"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/inspector"
    "${JAVASCRIPTCORE_DIR}/inspector/agents"
    "${JAVASCRIPTCORE_DIR}/inspector/augmentable"
    "${JAVASCRIPTCORE_DIR}/inspector/remote"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${JAVASCRIPTCORE_DIR}/tools"
    "${JAVASCRIPTCORE_DIR}/wasm"
    "${JAVASCRIPTCORE_DIR}/wasm/js"
    "${JAVASCRIPTCORE_DIR}/yarr"
    "${FORWARDING_HEADERS_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/runtime"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr"
)

set(JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
)

set(JavaScriptCore_OBJECT_LUT_SOURCES
    runtime/ArrayConstructor.cpp
    runtime/ArrayIteratorPrototype.cpp
    runtime/AsyncGeneratorPrototype.cpp
    runtime/BigIntConstructor.cpp
    runtime/BigIntPrototype.cpp
    runtime/BooleanPrototype.cpp
    runtime/DateConstructor.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorPrototype.cpp
    runtime/GeneratorPrototype.cpp
    runtime/InspectorInstrumentationObject.cpp
    runtime/IntlCollatorConstructor.cpp
    runtime/IntlCollatorPrototype.cpp
    runtime/IntlDateTimeFormatConstructor.cpp
    runtime/IntlDateTimeFormatPrototype.cpp
    runtime/IntlNumberFormatConstructor.cpp
    runtime/IntlNumberFormatPrototype.cpp
    runtime/JSDataViewPrototype.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSInternalPromiseConstructor.cpp
    runtime/JSONObject.cpp
    runtime/JSPromiseConstructor.cpp
    runtime/JSPromisePrototype.cpp
    runtime/MapPrototype.cpp
    runtime/ModuleLoaderPrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/ReflectObject.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpPrototype.cpp
    runtime/SetPrototype.cpp
    runtime/StringConstructor.cpp
    runtime/StringIteratorPrototype.cpp
    runtime/StringPrototype.cpp
    runtime/SymbolConstructor.cpp
    runtime/SymbolPrototype.cpp

    wasm/js/WebAssemblyCompileErrorConstructor.cpp
    wasm/js/WebAssemblyCompileErrorPrototype.cpp
    wasm/js/WebAssemblyInstanceConstructor.cpp
    wasm/js/WebAssemblyInstancePrototype.cpp
    wasm/js/WebAssemblyLinkErrorConstructor.cpp
    wasm/js/WebAssemblyLinkErrorPrototype.cpp
    wasm/js/WebAssemblyMemoryConstructor.cpp
    wasm/js/WebAssemblyMemoryPrototype.cpp
    wasm/js/WebAssemblyModuleConstructor.cpp
    wasm/js/WebAssemblyModulePrototype.cpp
    wasm/js/WebAssemblyPrototype.cpp
    wasm/js/WebAssemblyRuntimeErrorConstructor.cpp
    wasm/js/WebAssemblyRuntimeErrorPrototype.cpp
    wasm/js/WebAssemblyTableConstructor.cpp
    wasm/js/WebAssemblyTablePrototype.cpp
)

set(JavaScriptCore_LIBRARIES
    WTF${DEBUG_SUFFIX}
    ${ICU_I18N_LIBRARIES}
    ${LLVM_LIBRARIES}
)

set(JavaScriptCore_SCRIPTS_SOURCES_DIR "${JAVASCRIPTCORE_DIR}/Scripts")

# Globbing relies on the fact that generator-specific file names are prefixed with their directory.
# Top-level scripts should have a file extension, since they are invoked during the build.

set(JavaScriptCore_SCRIPTS_SOURCES_PATHS
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/*.pl
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/*.py
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/builtins/builtins*.py
)

# Force JavaScriptCore to run scripts from the same staging path as WebCore.
set(JavaScriptCore_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts")

file(MAKE_DIRECTORY ${JavaScriptCore_SCRIPTS_DIR})

# The directory flattening performed below mirrors what the Mac port does with private headers.

file(GLOB JavaScriptCore_SCRIPTS_SOURCES ${JavaScriptCore_SCRIPTS_SOURCES_PATHS})

foreach (_file ${JavaScriptCore_SCRIPTS_SOURCES})
    get_filename_component(_script "${_file}" NAME)
    add_custom_command(
        OUTPUT ${JavaScriptCore_SCRIPTS_DIR}/${_script}
        MAIN_DEPENDENCY ${_file}
        WORKING_DIRECTORY ${DERIVED_SOURCES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file} ${JavaScriptCore_SCRIPTS_DIR}/${_script}
        VERBATIM)
    list(APPEND JavaScriptCore_SCRIPTS ${JavaScriptCore_SCRIPTS_DIR}/${_script})
endforeach ()

set(UDIS_GEN_DEP
    disassembler/udis86/ud_opcode.py
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.c ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.h
    DEPENDS ${UDIS_GEN_DEP}
    WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/disassembler/udis86/ud_itab.py ${JAVASCRIPTCORE_DIR}/disassembler/udis86/optable.xml ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    VERBATIM)

list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/udis86_itab.h
)

set(LLINT_ASM
    llint/LowLevelInterpreter.asm
    llint/LowLevelInterpreter32_64.asm
    llint/LowLevelInterpreter64.asm
)

set(OFFLINE_ASM
    offlineasm/arm.rb
    offlineasm/arm64.rb
    offlineasm/ast.rb
    offlineasm/backends.rb
    offlineasm/cloop.rb
    offlineasm/config.rb
    offlineasm/instructions.rb
    offlineasm/mips.rb
    offlineasm/offsets.rb
    offlineasm/opt.rb
    offlineasm/parser.rb
    offlineasm/registers.rb
    offlineasm/risc.rb
    offlineasm/self_hash.rb
    offlineasm/settings.rb
    offlineasm/transform.rb
    offlineasm/x86.rb
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Bytecodes.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InitBytecodes.asm ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/BytecodeStructs.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/generate-bytecode-files
    DEPENDS ${JAVASCRIPTCORE_DIR}/generate-bytecode-files bytecode/BytecodeList.json
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/generate-bytecode-files --bytecodes_h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Bytecodes.h --init_bytecodes_asm ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InitBytecodes.asm --bytecode_structs_h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/BytecodeStructs.h ${JAVASCRIPTCORE_DIR}/bytecode/BytecodeList.json
    VERBATIM)

list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Bytecodes.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/BytecodeStructs.h
)

if (WIN32)
  set(OFFLINE_ASM_BACKEND "X86_WIN, X86_64_WIN, C_LOOP")
else ()
    if (WTF_CPU_X86)
        set(OFFLINE_ASM_BACKEND "X86")
    elseif (WTF_CPU_X86_64)
        set(OFFLINE_ASM_BACKEND "X86_64")
    elseif (WTF_CPU_ARM64)
        set(OFFLINE_ASM_BACKEND "ARM64")
    elseif (ARM_THUMB2_DETECTED)
        set(OFFLINE_ASM_BACKEND "ARMv7")
    elseif (ARM_TRADITIONAL_DETECTED)
        set(OFFLINE_ASM_BACKEND "ARMv7_TRADITIONAL")
    elseif (WTF_CPU_MIPS)
        set(OFFLINE_ASM_BACKEND "MIPS")
    endif ()

    if (NOT ENABLE_JIT)
        set(OFFLINE_ASM_BACKEND "C_LOOP")
    endif ()
endif ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb
    DEPENDS ${LLINT_ASM} ${OFFLINE_ASM} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InitBytecodes.asm
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb -I${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/ ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h ${OFFLINE_ASM_BACKEND}
    VERBATIM)

# We add the header files directly to the ADD_EXECUTABLE call instead of setting the
# OBJECT_DEPENDS property in LLIntOffsetsExtractor.cpp because generate_offset_extractor.rb and
# generate-bytecode-files may not regenerate the .h files in case the hash it calculates does not change.
# In this case, if some of the dependencies specified in the ADD_CUSTOM_COMMANDs above have
# changed the command will always be called because the mtime of the .h files will
# always be older than that of their dependencies.
# Additionally, setting the OBJECT_DEPENDS property will make the .h files a Makefile
# dependency of both LLIntOffsetsExtractor and LLIntOffsetsExtractor.cpp, so the command will
# actually be run multiple times!
add_executable(LLIntOffsetsExtractor
    ${JAVASCRIPTCORE_DIR}/llint/LLIntOffsetsExtractor.cpp
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntDesiredOffsets.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Bytecodes.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/BytecodeStructs.h
)
target_link_libraries(LLIntOffsetsExtractor WTF)

# The build system will execute asm.rb every time LLIntOffsetsExtractor's mtime is newer than
# LLIntAssembly.h's mtime. The problem we have here is: asm.rb has some built-in optimization
# that generates a checksum of the LLIntOffsetsExtractor binary, if the checksum of the new
# LLIntOffsetsExtractor matches, no output is generated. To make this target consistent and avoid
# running this command for every build, we artificially update LLIntAssembly.h's mtime (using touch)
# after every asm.rb run.
if (MSVC)
    set(LLIntOutput LowLevelInterpreterWin.asm)
    set(OFFLINE_ASM_ARGS --assembler=MASM)
else ()
    set(LLIntOutput LLIntAssembly.h)
endif ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${LLIntOutput}
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb
    DEPENDS LLIntOffsetsExtractor ${LLINT_ASM} ${OFFLINE_ASM} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InitBytecodes.asm
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb -I${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/ ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm $<TARGET_FILE:LLIntOffsetsExtractor> ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${LLIntOutput} ${OFFLINE_ASM_ARGS}
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${LLIntOutput}
    WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
    VERBATIM)

# The explanation for not making LLIntAssembly.h part of the OBJECT_DEPENDS property of some of
# the .cpp files below is similar to the one in the previous comment. However, since these .cpp
# files are used to build JavaScriptCore itself, we can just add LLIntAssembly.h to JSC_HEADERS
# since it is used in the add_library() call at the end of this file.
if (MSVC)
    enable_language(ASM_MASM)
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Win32 needs /safeseh with assembly, but Win64 does not.
        set(MASM_EXECUTABLE ml)
        set(LLINT_MASM_FLAGS /safeseh /c /Fo)
    else ()
        set(MASM_EXECUTABLE ml64)
        set(LLINT_MASM_FLAGS /c /Fo)
    endif ()
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LowLevelInterpreterWin.obj
        DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LowLevelInterpreterWin.asm
        COMMAND ${MASM_EXECUTABLE} ${LLINT_MASM_FLAGS} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LowLevelInterpreterWin.obj ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LowLevelInterpreterWin.asm
        VERBATIM)
    list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LowLevelInterpreterWin.obj)
else ()
    list(APPEND JavaScriptCore_HEADERS
        ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/LLIntAssembly.h
    )
endif ()

# WebAssembly generator

macro(GENERATE_PYTHON _generator _additional_deps _input _output)
    add_custom_command(
        OUTPUT ${_output}
        MAIN_DEPENDENCY ${_generator}
        DEPENDS ${_input} ${_additional_deps}
        COMMAND ${PYTHON_EXECUTABLE} ${_generator} ${_input} ${_output}
        VERBATIM)
    list(APPEND JavaScriptCore_HEADERS ${_output})
    WEBKIT_ADD_SOURCE_DEPENDENCIES(${_input} ${_output})
endmacro()
GENERATE_PYTHON(${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasmOpsHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasm.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm.json ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WasmOps.h)
GENERATE_PYTHON(${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasmValidateInlinesHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasm.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm.json ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WasmValidateInlines.h)
GENERATE_PYTHON(${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasmB3IRGeneratorInlinesHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasm.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm.json ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/WasmB3IRGeneratorInlines.h)

# LUT generator

set(HASH_LUT_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/create_hash_table)
macro(GENERATE_HASH_LUT _input _output)
    add_custom_command(
        OUTPUT ${_output}
        MAIN_DEPENDENCY ${HASH_LUT_GENERATOR}
        DEPENDS ${_input}
        COMMAND ${PERL_EXECUTABLE} ${HASH_LUT_GENERATOR} ${_input} > ${_output}
        VERBATIM)
    list(APPEND JavaScriptCore_HEADERS ${_output})
    WEBKIT_ADD_SOURCE_DEPENDENCIES(${_input} ${_output})
endmacro()

# GENERATOR 1-A: LUT creator

foreach (_file ${JavaScriptCore_OBJECT_LUT_SOURCES})
    get_filename_component(_name ${_file} NAME_WE)
    GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/${_file} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/${_name}.lut.h)
endforeach ()

set(JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES
    API
    assembler
    bindings
    builtins
    bytecode
    debugger
    dfg
    disassembler
    domjit
    heap
    inspector
    interpreter
    jit
    llint
    parser
    profiler
    runtime
    wasm
    yarr

    collector/handles

    inspector/agents
    inspector/augmentable
    inspector/remote

    heap/SecurityOriginToken.h

    wasm/js
)

# GENERATOR 1-B: particular LUT creator (for 1 file only)
GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/Lexer.lut.h)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/RegExpJitTables.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/yarr/create_regex_tables
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/yarr/create_regex_tables ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/RegExpJitTables.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/RegExpJitTables.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/yarr/YarrPattern.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/RegExpJitTables.h)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/UnicodePatternTables.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/Scripts/generateYarrUnicodePropertyTables.py
    DEPENDS ${JAVASCRIPTCORE_DIR}/Scripts/hasher.py ${JAVASCRIPTCORE_DIR}/ucd/DerivedBinaryProperties.txt ${JAVASCRIPTCORE_DIR}/ucd/DerivedCoreProperties.txt ${JAVASCRIPTCORE_DIR}/ucd/DerivedNormalizationProps.txt ${JAVASCRIPTCORE_DIR}/ucd/PropList.txt ${JAVASCRIPTCORE_DIR}/ucd/PropertyAliases.txt ${JAVASCRIPTCORE_DIR}/ucd/PropertyValueAliases.txt ${JAVASCRIPTCORE_DIR}/ucd/ScriptExtensions.txt ${JAVASCRIPTCORE_DIR}/ucd/Scripts.txt ${JAVASCRIPTCORE_DIR}/ucd/UnicodeData.txt ${JAVASCRIPTCORE_DIR}/ucd/emoji-data.txt
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/Scripts/generateYarrUnicodePropertyTables.py ${JAVASCRIPTCORE_DIR}/ucd ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/UnicodePatternTables.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/UnicodePatternTables.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/yarr/UnicodePatternTables.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/UnicodePatternTables.h)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/YarrCanonicalizeUnicode.cpp
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/yarr/generateYarrCanonicalizeUnicode
    DEPENDS ${JAVASCRIPTCORE_DIR}/ucd/CaseFolding.txt
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/yarr/generateYarrCanonicalizeUnicode ${JAVASCRIPTCORE_DIR}/ucd/CaseFolding.txt ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/yarr/YarrCanonicalizeUnicode.cpp
    VERBATIM)

#GENERATOR: "KeywordLookup.h": keyword decision tree used by the lexer
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py ${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/parser/Lexer.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/KeywordLookup.h)


# Inspector Interfaces

set(JavaScriptCore_INSPECTOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

set(JavaScriptCore_INSPECTOR_PROTOCOL_SCRIPTS
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/cpp_generator.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/cpp_generator_templates.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_js_backend_commands.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_header.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_implementation.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_header.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_implementation.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_header.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_implementation.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generator.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/generator_templates.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/__init__.py
    ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/codegen/models.py
)

set(JavaScriptCore_INSPECTOR_DOMAINS
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/ApplicationCache.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/CSS.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Canvas.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Console.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/DOM.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/DOMDebugger.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/DOMStorage.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Database.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Debugger.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/GenericTypes.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Heap.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Inspector.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/LayerTree.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Network.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/OverlayTypes.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Page.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Recording.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Runtime.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/ScriptProfiler.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Timeline.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Worker.json
)

if (ENABLE_INDEXED_DATABASE)
    list(APPEND JavaScriptCore_INSPECTOR_DOMAINS
        ${JAVASCRIPTCORE_DIR}/inspector/protocol/IndexedDB.json
    )
endif ()

if (ENABLE_RESOURCE_USAGE)
    list(APPEND JavaScriptCore_INSPECTOR_DOMAINS
        ${JAVASCRIPTCORE_DIR}/inspector/protocol/Memory.json
    )
endif ()

if (ENABLE_SERVICE_WORKER)
    list(APPEND JavaScriptCore_INSPECTOR_DOMAINS
        ${JAVASCRIPTCORE_DIR}/inspector/protocol/ServiceWorker.json
    )
endif ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/CombinedDomains.json
    MAIN_DEPENDENCY ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py
    DEPENDS ${JavaScriptCore_INSPECTOR_DOMAINS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py ${JavaScriptCore_INSPECTOR_DOMAINS} > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/CombinedDomains.json
    VERBATIM)

# Inspector Backend Dispatchers, Frontend Dispatchers, Type Builders
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol)
file(MAKE_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendDispatchers.cpp
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendDispatchers.h
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorFrontendDispatchers.cpp
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorFrontendDispatchers.h
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorProtocolObjects.cpp
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorProtocolObjects.h
           ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    MAIN_DEPENDENCY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/CombinedDomains.json
    DEPENDS ${JavaScriptCore_INSPECTOR_PROTOCOL_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector" --framework JavaScriptCore ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/CombinedDomains.json
    VERBATIM)

# JSCBuiltins

set(BUILTINS_GENERATOR_SCRIPTS
    ${JavaScriptCore_SCRIPTS_DIR}/builtins.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generator.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_model.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_templates.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py
    ${JavaScriptCore_SCRIPTS_DIR}/lazywriter.py
)

set(JavaScriptCore_BUILTINS_SOURCES
    ${JAVASCRIPTCORE_DIR}/builtins/AsyncFromSyncIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ArrayConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/ArrayIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ArrayPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/AsyncFunctionPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/AsyncIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/AsyncGeneratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/DatePrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/FunctionPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/GeneratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/GlobalObject.js
    ${JAVASCRIPTCORE_DIR}/builtins/GlobalOperations.js
    ${JAVASCRIPTCORE_DIR}/builtins/InspectorInstrumentationObject.js
    ${JAVASCRIPTCORE_DIR}/builtins/InternalPromiseConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/IteratorHelpers.js
    ${JAVASCRIPTCORE_DIR}/builtins/IteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/MapIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/MapPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ModuleLoaderPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/NumberConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/NumberPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ObjectConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromiseConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromiseOperations.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromisePrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ReflectObject.js
    ${JAVASCRIPTCORE_DIR}/builtins/RegExpPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/SetIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/SetPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/TypedArrayConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/TypedArrayPrototype.js
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.cpp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h
    MAIN_DEPENDENCY ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py
    DEPENDS ${JavaScriptCore_BUILTINS_SOURCES} ${BUILTINS_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --framework JavaScriptCore --output-directory ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR} --combined ${JavaScriptCore_BUILTINS_SOURCES}
    VERBATIM)

list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendDispatchers.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorFrontendDispatchers.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorProtocolObjects.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/JSCBuiltins.h
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/AirOpcode.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/AirOpcodeGenerated.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/b3/air/AirOpcode.opcodes
    DEPENDS ${JAVASCRIPTCORE_DIR}/b3/air/opcode_generator.rb
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/b3/air/opcode_generator.rb ${JAVASCRIPTCORE_DIR}/b3/air/AirOpcode.opcodes VERBATIM
    WORKING_DIRECTORY ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
)

list(APPEND JavaScriptCore_HEADERS
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/AirOpcode.h
    ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/AirOpcodeGenerated.h
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/inspector/InjectedScriptSource.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_InjectedScriptSource.js" > ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${JAVASCRIPTCORE_DIR}/inspector/InjectedScriptSource.js >> ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl InjectedScriptSource_js ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.min.js ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h
    VERBATIM)

list(APPEND JavaScriptCore_HEADERS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/InjectedScriptSource.h)

if (WTF_CPU_X86_64)
    if (MSVC AND ENABLE_JIT)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj
            MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/jit/JITStubsMSVC64.asm
            COMMAND ml64 -nologo -c -Fo ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj ${JAVASCRIPTCORE_DIR}/jit/JITStubsMSVC64.asm
            VERBATIM)

        list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_DIR}/JITStubsMSVC64.obj)
    endif ()
endif ()

WEBKIT_FRAMEWORK_DECLARE(JavaScriptCore)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_CREATE_FORWARDING_HEADERS(JavaScriptCore DIRECTORIES ${JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${JavaScriptCore_FORWARDING_HEADERS_FILES})

target_include_directories(LLIntOffsetsExtractor PRIVATE ${JavaScriptCore_INCLUDE_DIRECTORIES})

add_subdirectory(shell)

WEBKIT_COMPUTE_SOURCES(JavaScriptCore)
WEBKIT_WRAP_SOURCELIST(${JavaScriptCore_SOURCES})
WEBKIT_FRAMEWORK(JavaScriptCore)

if (NOT "${PORT}" STREQUAL "Mac")
    if (${JavaScriptCore_LIBRARY_TYPE} STREQUAL "SHARED")
        WEBKIT_POPULATE_LIBRARY_VERSION(JAVASCRIPTCORE)
        set_target_properties(JavaScriptCore PROPERTIES VERSION ${JAVASCRIPTCORE_VERSION} SOVERSION ${JAVASCRIPTCORE_VERSION_MAJOR})
        install(TARGETS JavaScriptCore DESTINATION "${LIB_INSTALL_DIR}")
    endif ()
endif ()

# Force staging of shared scripts, even if they aren't directly used to build JavaScriptCore.

add_custom_target(stageSharedScripts DEPENDS ${JavaScriptCore_SCRIPTS})
add_dependencies(JavaScriptCore stageSharedScripts)

if (MSVC)
    add_custom_command(
        TARGET JavaScriptCore
        PRE_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/auto-version.pl ${DERIVED_SOURCES_DIR}
        VERBATIM)

    add_custom_command(
        TARGET JavaScriptCore
        POST_BUILD
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/version-stamp.pl ${DERIVED_SOURCES_DIR} $<TARGET_FILE:JavaScriptCore>
        VERBATIM)
endif ()
