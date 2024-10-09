set_property(DIRECTORY . PROPERTY FOLDER "JavaScriptCore")

list(APPEND JavaScriptCore_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(JavaScriptCore_INCLUDE_DIRECTORIES
    "${JavaScriptCore_FRAMEWORK_HEADERS_DIR}"
)

set(JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/b3"
    "${JAVASCRIPTCORE_DIR}/b3/air"
    "${JAVASCRIPTCORE_DIR}/bindings"
    "${JAVASCRIPTCORE_DIR}/builtins"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/disassembler/ARM64"
    "${JAVASCRIPTCORE_DIR}/disassembler/zydis/Zydis"
    "${JAVASCRIPTCORE_DIR}/domjit"
    "${JAVASCRIPTCORE_DIR}/ftl"
    "${JAVASCRIPTCORE_DIR}/fuzzilli"
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
    "${JavaScriptCore_DERIVED_SOURCES_DIR}"
    "${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector"
    "${JavaScriptCore_DERIVED_SOURCES_DIR}/runtime"
    "${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr"
)

if (USE_CAPSTONE)
    list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/capstone/Source/include")
endif ()

set(JavaScriptCore_OBJECT_LUT_SOURCES
    runtime/ArrayConstructor.cpp
    runtime/AsyncFromSyncIteratorPrototype.cpp
    runtime/AsyncGeneratorPrototype.cpp
    runtime/BigIntConstructor.cpp
    runtime/BigIntPrototype.cpp
    runtime/BooleanPrototype.cpp
    runtime/DateConstructor.cpp
    runtime/DatePrototype.cpp
    runtime/ErrorPrototype.cpp
    runtime/GeneratorPrototype.cpp
    runtime/IntlCollatorConstructor.cpp
    runtime/IntlCollatorPrototype.cpp
    runtime/IntlDateTimeFormatConstructor.cpp
    runtime/IntlDateTimeFormatPrototype.cpp
    runtime/IntlDisplayNamesConstructor.cpp
    runtime/IntlDisplayNamesPrototype.cpp
    runtime/IntlDurationFormatConstructor.cpp
    runtime/IntlDurationFormatPrototype.cpp
    runtime/IntlListFormatConstructor.cpp
    runtime/IntlListFormatPrototype.cpp
    runtime/IntlLocalePrototype.cpp
    runtime/IntlNumberFormatConstructor.cpp
    runtime/IntlNumberFormatPrototype.cpp
    runtime/IntlObject.cpp
    runtime/IntlPluralRulesConstructor.cpp
    runtime/IntlPluralRulesPrototype.cpp
    runtime/IntlRelativeTimeFormatConstructor.cpp
    runtime/IntlRelativeTimeFormatPrototype.cpp
    runtime/IntlSegmentIteratorPrototype.cpp
    runtime/IntlSegmenterConstructor.cpp
    runtime/IntlSegmenterPrototype.cpp
    runtime/IntlSegmentsPrototype.cpp
    runtime/JSDataViewPrototype.cpp
    runtime/JSGlobalObject.cpp
    runtime/JSInternalPromiseConstructor.cpp
    runtime/JSIterator.cpp
    runtime/JSIteratorConstructor.cpp
    runtime/JSIteratorHelperPrototype.cpp
    runtime/JSONObject.cpp
    runtime/JSPromiseConstructor.cpp
    runtime/JSPromisePrototype.cpp
    runtime/MapConstructor.cpp
    runtime/MapPrototype.cpp
    runtime/NumberConstructor.cpp
    runtime/NumberPrototype.cpp
    runtime/ObjectConstructor.cpp
    runtime/ReflectObject.cpp
    runtime/RegExpConstructor.cpp
    runtime/RegExpPrototype.cpp
    runtime/RegExpStringIteratorPrototype.cpp
    runtime/SetPrototype.cpp
    runtime/ShadowRealmObject.cpp
    runtime/ShadowRealmPrototype.cpp
    runtime/StringConstructor.cpp
    runtime/StringPrototype.cpp
    runtime/SymbolConstructor.cpp
    runtime/SymbolPrototype.cpp
    runtime/TemporalCalendarConstructor.cpp
    runtime/TemporalCalendarPrototype.cpp
    runtime/TemporalDurationConstructor.cpp
    runtime/TemporalDurationPrototype.cpp
    runtime/TemporalInstantConstructor.cpp
    runtime/TemporalInstantPrototype.cpp
    runtime/TemporalNow.cpp
    runtime/TemporalObject.cpp
    runtime/TemporalPlainDateConstructor.cpp
    runtime/TemporalPlainDatePrototype.cpp
    runtime/TemporalPlainDateTimeConstructor.cpp
    runtime/TemporalPlainDateTimePrototype.cpp
    runtime/TemporalPlainTimeConstructor.cpp
    runtime/TemporalPlainTimePrototype.cpp
    runtime/TemporalTimeZoneConstructor.cpp
    runtime/TemporalTimeZonePrototype.cpp

    wasm/js/JSWebAssembly.cpp
    wasm/js/WebAssemblyArrayConstructor.cpp
    wasm/js/WebAssemblyArrayPrototype.cpp
    wasm/js/WebAssemblyCompileErrorConstructor.cpp
    wasm/js/WebAssemblyCompileErrorPrototype.cpp
    wasm/js/WebAssemblyExceptionConstructor.cpp
    wasm/js/WebAssemblyExceptionPrototype.cpp
    wasm/js/WebAssemblyGlobalConstructor.cpp
    wasm/js/WebAssemblyGlobalPrototype.cpp
    wasm/js/WebAssemblyInstanceConstructor.cpp
    wasm/js/WebAssemblyInstancePrototype.cpp
    wasm/js/WebAssemblyLinkErrorConstructor.cpp
    wasm/js/WebAssemblyLinkErrorPrototype.cpp
    wasm/js/WebAssemblyMemoryConstructor.cpp
    wasm/js/WebAssemblyMemoryPrototype.cpp
    wasm/js/WebAssemblyModuleConstructor.cpp
    wasm/js/WebAssemblyModulePrototype.cpp
    wasm/js/WebAssemblyRuntimeErrorConstructor.cpp
    wasm/js/WebAssemblyRuntimeErrorPrototype.cpp
    wasm/js/WebAssemblyStructConstructor.cpp
    wasm/js/WebAssemblyStructPrototype.cpp
    wasm/js/WebAssemblyTableConstructor.cpp
    wasm/js/WebAssemblyTablePrototype.cpp
    wasm/js/WebAssemblyTagConstructor.cpp
    wasm/js/WebAssemblyTagPrototype.cpp
)

list(APPEND JavaScriptCore_SOURCES
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/JSCBuiltins.cpp
)

set(JavaScriptCore_FRAMEWORKS
    WTF
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND JavaScriptCore_FRAMEWORKS bmalloc)
endif ()

if (USE_CAPSTONE)
    list(APPEND JavaScriptCore_LIBRARIES capstone)
endif ()

if (ATOMICS_REQUIRE_LIBATOMIC)
    list(APPEND JavaScriptCore_LIBRARIES atomic)
endif ()

set(JavaScriptCore_SCRIPTS_SOURCES_DIR "${JAVASCRIPTCORE_DIR}/Scripts")

# Globbing relies on the fact that generator-specific file names are prefixed with their directory.
# Top-level scripts should have a file extension, since they are invoked during the build.

set(JavaScriptCore_SCRIPTS_SOURCES_PATHS
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/*.pl
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/*.py
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/wkbuiltins/builtins*.py
    ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/wkbuiltins/wkbuiltins.py
)

# The directory flattening performed below mirrors what the Mac port does with private headers.

file(GLOB JavaScriptCore_SCRIPTS_SOURCES ${JavaScriptCore_SCRIPTS_SOURCES_PATHS})

foreach (_file ${JavaScriptCore_SCRIPTS_SOURCES})
    get_filename_component(_script "${_file}" NAME)
    add_custom_command(
        OUTPUT ${JavaScriptCore_SCRIPTS_DIR}/${_script}
        MAIN_DEPENDENCY ${_file}
        WORKING_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_file} ${JavaScriptCore_SCRIPTS_DIR}/${_script}
        VERBATIM)
    list(APPEND JavaScriptCore_SCRIPTS ${JavaScriptCore_SCRIPTS_DIR}/${_script})
endforeach ()

# This is the default build variant for Xcode builds.
set(BUILD_VARIANTS
    "normal"
)

set(LLINT_ASM
    llint/InPlaceInterpreter.asm
    llint/InPlaceInterpreter32_64.asm
    llint/InPlaceInterpreter64.asm
    llint/LowLevelInterpreter.asm
    llint/LowLevelInterpreter32_64.asm
    llint/LowLevelInterpreter64.asm
    llint/WebAssembly.asm
    llint/WebAssembly32_64.asm
    llint/WebAssembly64.asm
)

set(OFFLINE_ASM
    offlineasm/arm.rb
    offlineasm/arm64.rb
    offlineasm/arm64e.rb
    offlineasm/asm.rb
    offlineasm/ast.rb
    offlineasm/backends.rb
    offlineasm/cloop.rb
    offlineasm/config.rb
    offlineasm/instructions.rb
    offlineasm/offsets.rb
    offlineasm/opt.rb
    offlineasm/parser.rb
    offlineasm/registers.rb
    offlineasm/risc.rb
    offlineasm/riscv64.rb
    offlineasm/self_hash.rb
    offlineasm/settings.rb
    offlineasm/transform.rb
    offlineasm/x86.rb
)

set(GENERATOR
    generator/Argument.rb
    generator/Assertion.rb
    generator/Checkpoints.rb
    generator/DSL.rb
    generator/Fits.rb
    generator/GeneratedFile.rb
    generator/Metadata.rb
    generator/Opcode.rb
    generator/OpcodeGroup.rb
    generator/Options.rb
    generator/Section.rb
    generator/Template.rb
    generator/Type.rb
    generator/Wasm.rb
    generator/main.rb
)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitBytecodes.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeStructs.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeIndices.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmLLIntGeneratorInlines.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitWasm.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeDumperGenerated.cpp
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/generator/main.rb
    DEPENDS ${GENERATOR} bytecode/BytecodeList.rb ${JAVASCRIPTCORE_DIR}/wasm/wasm.json
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/generator/main.rb --bytecodes_h ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h --init_bytecodes_asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitBytecodes.asm --bytecode_structs_h ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeStructs.h --bytecode_indices_h ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeIndices.h ${JAVASCRIPTCORE_DIR}/bytecode/BytecodeList.rb --wasm_json ${JAVASCRIPTCORE_DIR}/wasm/wasm.json --wasm_llint_generator_h ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmLLIntGeneratorInlines.h --init_wasm_llint ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitWasm.asm --bytecode_dumper ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeDumperGenerated.cpp
    VERBATIM)
add_custom_target(Bytecodes DEPENDS "${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h")

string(TIMESTAMP BUILD_TIME "%s")

list(APPEND JavaScriptCore_HEADERS
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeStructs.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h
)

if (WTF_CPU_X86_64)
    set(OFFLINE_ASM_BACKEND "X86_64")
elseif (WTF_CPU_ARM64)
    set(OFFLINE_ASM_BACKEND "ARM64")
elseif (ARM_THUMB2_DETECTED)
    set(OFFLINE_ASM_BACKEND "ARMv7")
elseif (WTF_CPU_RISCV64)
    set(OFFLINE_ASM_BACKEND "RISCV64")
endif ()

if (NOT ENABLE_JIT)
    if (ENABLE_C_LOOP)
        set(OFFLINE_ASM_BACKEND "C_LOOP")
    endif ()
endif ()

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcode.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcodeGenerated.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/b3/air/AirOpcode.opcodes
    DEPENDS ${JAVASCRIPTCORE_DIR}/b3/air/opcode_generator.rb
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/b3/air/opcode_generator.rb ${JAVASCRIPTCORE_DIR}/b3/air/AirOpcode.opcodes VERBATIM
    WORKING_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR}
)

list(APPEND JavaScriptCore_HEADERS
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcode.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcodeGenerated.h
)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredSettings.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/generate_settings_extractor.rb
    DEPENDS ${LLINT_ASM} ${OFFLINE_ASM} ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitBytecodes.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitWasm.asm
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/generate_settings_extractor.rb -I${JavaScriptCore_DERIVED_SOURCES_DIR}/ ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredSettings.h ${OFFLINE_ASM_BACKEND}
    VERBATIM)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredOffsets.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb
    DEPENDS LLIntSettingsExtractor ${LLINT_ASM} ${OFFLINE_ASM} ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitBytecodes.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitWasm.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/AirOpcode.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmOps.h
    COMMAND ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/generate_offset_extractor.rb -I${JavaScriptCore_DERIVED_SOURCES_DIR}/ ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm $<TARGET_FILE:LLIntSettingsExtractor> ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredOffsets.h ${OFFLINE_ASM_BACKEND} ${BUILD_VARIANTS}
    VERBATIM)

# JSCBuiltins

set(BUILTINS_GENERATOR_SCRIPTS
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins.py
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
    ${JAVASCRIPTCORE_DIR}/builtins/AsyncGeneratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/FunctionPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/GeneratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/GlobalObject.js
    ${JAVASCRIPTCORE_DIR}/builtins/GlobalOperations.js
    ${JAVASCRIPTCORE_DIR}/builtins/InternalPromiseConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/IteratorHelpers.js
    ${JAVASCRIPTCORE_DIR}/builtins/JSIteratorConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/JSIteratorHelperPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/JSIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/MapConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/MapIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/MapPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ModuleLoader.js
    ${JAVASCRIPTCORE_DIR}/builtins/NumberConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/ObjectConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromiseConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromiseOperations.js
    ${JAVASCRIPTCORE_DIR}/builtins/PromisePrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ProxyHelpers.js
    ${JAVASCRIPTCORE_DIR}/builtins/ReflectObject.js
    ${JAVASCRIPTCORE_DIR}/builtins/RegExpPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/RegExpStringIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/SetIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/SetPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/ShadowRealmPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/StringPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/TypedArrayConstructor.js
    ${JAVASCRIPTCORE_DIR}/builtins/TypedArrayPrototype.js
    ${JAVASCRIPTCORE_DIR}/builtins/WebAssembly.js
    ${JAVASCRIPTCORE_DIR}/builtins/WrapForValidIteratorPrototype.js
    ${JAVASCRIPTCORE_DIR}/inspector/InjectedScriptSource.js
)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/JSCBuiltins.cpp ${JavaScriptCore_DERIVED_SOURCES_DIR}/JSCBuiltins.h
    MAIN_DEPENDENCY ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py
    DEPENDS ${JavaScriptCore_BUILTINS_SOURCES} ${BUILTINS_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --framework JavaScriptCore --output-directory ${JavaScriptCore_DERIVED_SOURCES_DIR} --combined ${JavaScriptCore_BUILTINS_SOURCES}
    VERBATIM)
add_custom_target(JSCBuiltins DEPENDS "${JavaScriptCore_DERIVED_SOURCES_DIR}/JSCBuiltins.h")

# We add the header files directly to the ADD_EXECUTABLE call instead of setting the
# OBJECT_DEPENDS property in LLIntOffsetsExtractor.cpp because generate_offset_extractor.rb and
# generate-bytecode-files may not regenerate the .h files in case the hash it calculates does not change.
# In this case, if some of the dependencies specified in the ADD_CUSTOM_COMMANDs above have
# changed the command will always be called because the mtime of the .h files will
# always be older than that of their dependencies.
# Additionally, setting the OBJECT_DEPENDS property will make the .h files a Makefile
# dependency of both LLIntOffsetsExtractor and LLIntOffsetsExtractor.cpp, so the command will
# actually be run multiple times!

WEBKIT_EXECUTABLE_DECLARE(LLIntSettingsExtractor)
set(LLIntSettingsExtractor_SOURCES
    ${JAVASCRIPTCORE_DIR}/llint/LLIntSettingsExtractor.cpp
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredSettings.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeStructs.h
)

set(LLIntSettingsExtractor_PRIVATE_INCLUDE_DIRECTORIES
    $<TARGET_PROPERTY:JavaScriptCore,INCLUDE_DIRECTORIES>
)
set(LLIntSettingsExtractor_FRAMEWORKS ${JavaScriptCore_FRAMEWORKS})
set(LLIntSettingsExtractor_DEPENDENCIES JavaScriptCore_CopyHeaders)
WEBKIT_EXECUTABLE(LLIntSettingsExtractor)

# LLIntSettingsExtractor target needs to have a direct or indirect
# dependency to Bytecodes target for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise, Bytecodes.h will be
# generated in both projects.
add_dependencies(LLIntSettingsExtractor Bytecodes)

WEBKIT_EXECUTABLE_DECLARE(LLIntOffsetsExtractor)
set(LLIntOffsetsExtractor_SOURCES
    ${JAVASCRIPTCORE_DIR}/llint/LLIntOffsetsExtractor.cpp
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/LLIntDesiredOffsets.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h ${JavaScriptCore_DERIVED_SOURCES_DIR}/BytecodeStructs.h
)

set(LLIntOffsetsExtractor_PRIVATE_INCLUDE_DIRECTORIES
    $<TARGET_PROPERTY:JavaScriptCore,INCLUDE_DIRECTORIES>
)
set(LLIntOffsetsExtractor_FRAMEWORKS ${JavaScriptCore_FRAMEWORKS})
set(LLIntOffsetsExtractor_DEPENDENCIES JavaScriptCore_CopyHeaders JSCBuiltins)
WEBKIT_EXECUTABLE(LLIntOffsetsExtractor)

# The build system will execute asm.rb every time LLIntOffsetsExtractor's mtime is newer than
# LLIntAssembly.h's mtime. The problem we have here is: asm.rb has some built-in optimization
# that generates a checksum of the LLIntOffsetsExtractor binary, if the checksum of the new
# LLIntOffsetsExtractor matches, no output is generated. To make this target consistent and avoid
# running this command for every build, we artificially update LLIntAssembly.h's mtime (using touch)
# after every asm.rb run.
set(LLIntOutput LLIntAssembly.h)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(OFFLINE_ASM_ARGS --binary-format=ELF)
endif ()

if (MSVC AND NOT ENABLE_C_LOOP)
    set(OFFLINE_ASM_ARGS --platform=Windows)
endif ()

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/${LLIntOutput}
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb
    DEPENDS LLIntOffsetsExtractor ${LLINT_ASM} ${OFFLINE_ASM} ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitBytecodes.asm ${JavaScriptCore_DERIVED_SOURCES_DIR}/InitWasm.asm
    COMMAND ${CMAKE_COMMAND} -E env CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID} GCC_OFFLINEASM_SOURCE_MAP=${GCC_OFFLINEASM_SOURCE_MAP} ${RUBY_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/offlineasm/asm.rb -I${JavaScriptCore_DERIVED_SOURCES_DIR}/ ${JAVASCRIPTCORE_DIR}/llint/LowLevelInterpreter.asm $<TARGET_FILE:LLIntOffsetsExtractor> ${JavaScriptCore_DERIVED_SOURCES_DIR}/${LLIntOutput} ${BUILD_VARIANTS} ${OFFLINE_ASM_ARGS}
    COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${JavaScriptCore_DERIVED_SOURCES_DIR}/${LLIntOutput}
    WORKING_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR}
    VERBATIM)

# The explanation for not making LLIntAssembly.h part of the OBJECT_DEPENDS property of some of
# the .cpp files below is similar to the one in the previous comment. However, since these .cpp
# files are used to build JavaScriptCore itself, we can just add LLIntAssembly.h to JSC_HEADERS
# since it is used in the add_library() call at the end of this file.

# As there's poor toolchain support for using `.file` directives in
# inline asm (i.e. there's no way to avoid clashes with the `.file`
# directives generated by the C code in the compilation unit), we
# introduce a postprocessing pass for the asm that gets assembled into
# an object file. We only need to do this for LowLevelInterpreter.cpp
# and cmake doesn't allow us to introduce a compiler wrapper for a
# single source file, so we need to create a separate target for it.
add_library(LowLevelInterpreterLib OBJECT llint/LowLevelInterpreter.cpp
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/${LLIntOutput})

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
GENERATE_PYTHON(${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasmOpsHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasm.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm.json ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmOps.h)
GENERATE_PYTHON(${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasmOMGIRGeneratorInlinesHeader.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/generateWasm.py ${CMAKE_CURRENT_SOURCE_DIR}/wasm/wasm.json ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmOMGIRGeneratorInlines.h)

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
    GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/${_file} ${JavaScriptCore_DERIVED_SOURCES_DIR}/${_name}.lut.h)
endforeach ()

set(JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS
    API/JSBase.h
    API/JSContextRef.h
    API/JSObjectRef.h
    API/JSStringRef.h
    API/JSTypedArray.h
    API/JSValueRef.h
    API/JavaScript.h
    API/WebKitAvailability.h
)

set(JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/Bytecodes.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/JSCBuiltins.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/WasmOps.h

    ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendDispatchers.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorFrontendDispatchers.h
    ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorProtocolObjects.h

    API/APICallbackFunction.h
    API/APICast.h
    API/APIUtils.h
    API/JSAPIValueWrapper.h
    API/JSAPIWrapperObject.h
    API/JSBasePrivate.h
    API/JSCTestRunnerUtils.h
    API/JSCallbackConstructor.h
    API/JSCallbackFunction.h
    API/JSCallbackObject.h
    API/JSCallbackObjectFunctions.h
    API/JSClassRef.h
    API/JSContextInternal.h
    API/JSContextPrivate.h
    API/JSContextRefInspectorSupport.h
    API/JSContextRefInternal.h
    API/JSContextRefPrivate.h
    API/JSHeapFinalizerPrivate.h
    API/JSManagedValueInternal.h
    API/JSMarkingConstraintPrivate.h
    API/JSObjectRefPrivate.h
    API/JSRemoteInspector.h
    API/JSRetainPtr.h
    API/JSScriptRefPrivate.h
    API/JSStringRefPrivate.h
    API/JSValueInternal.h
    API/JSValuePrivate.h
    API/JSVirtualMachineInternal.h
    API/JSWeakObjectMapRefInternal.h
    API/JSWeakObjectMapRefPrivate.h
    API/JSWeakPrivate.h
    API/JSWrapperMap.h
    API/ObjCCallbackFunction.h
    API/ObjcRuntimeExtras.h
    API/OpaqueJSString.h

    assembler/ARM64Assembler.h
    assembler/ARM64Registers.h
    assembler/ARMv7Assembler.h
    assembler/ARMv7Registers.h
    assembler/AbortReason.h
    assembler/AbstractMacroAssembler.h
    assembler/AllowMacroScratchRegisterUsage.h
    assembler/AssemblerBuffer.h
    assembler/AssemblerCommon.h
    assembler/AssemblyComments.h
    assembler/CPU.h
    assembler/CPUInlines.h
    assembler/CodeLocation.h
    assembler/FastJITPermissions.h
    assembler/JITOperationList.h
    assembler/JITOperationValidation.h
    assembler/LinkBuffer.h
    assembler/MacroAssembler.h
    assembler/MacroAssemblerARM64.h
    assembler/MacroAssemblerARMv7.h
    assembler/MacroAssemblerCodeRef.h
    assembler/MacroAssemblerHelpers.h
    assembler/MacroAssemblerRISCV64.h
    assembler/MacroAssemblerX86_64.h
    assembler/OSCheck.h
    assembler/Printer.h
    assembler/RISCV64Assembler.h
    assembler/RISCV64Registers.h
    assembler/RegisterInfo.h
    assembler/SecureARM64EHashPins.h
    assembler/SecureARM64EHashPinsInlines.h
    assembler/X86Assembler.h
    assembler/X86_64Registers.h

    b3/B3Common.h
    b3/B3Type.h
    b3/B3ValueRep.h

    builtins/BuiltinNames.h
    builtins/BuiltinUtils.h

    bytecode/ArithProfile.h
    bytecode/ArrayAllocationProfile.h
    bytecode/ArrayProfile.h
    bytecode/BytecodeConventions.h
    bytecode/BytecodeIndex.h
    bytecode/BytecodeIntrinsicRegistry.h
    bytecode/CallEdge.h
    bytecode/CallLinkInfo.h
    bytecode/CallLinkInfoBase.h
    bytecode/CallMode.h
    bytecode/CallVariant.h
    bytecode/CallVariantInlines.h
    bytecode/ChainedWatchpoint.h
    bytecode/CheckPrivateBrandStatus.h
    bytecode/CheckPrivateBrandVariant.h
    bytecode/CodeBlock.h
    bytecode/CodeBlockHash.h
    bytecode/CodeOrigin.h
    bytecode/CodeType.h
    bytecode/DFGExitProfile.h
    bytecode/DataFormat.h
    bytecode/DirectEvalCodeCache.h
    bytecode/ExecutableInfo.h
    bytecode/ExecutionCounter.h
    bytecode/ExitKind.h
    bytecode/ExitingInlineKind.h
    bytecode/ExitingJITType.h
    bytecode/ExpressionInfo.h
    bytecode/ExpressionInfoInlines.h
    bytecode/HandlerInfo.h
    bytecode/ICStatusMap.h
    bytecode/InlineCallFrame.h
    bytecode/Instruction.h
    bytecode/InstructionStream.h
    bytecode/InternalFunctionAllocationProfile.h
    bytecode/IterationModeMetadata.h
    bytecode/JumpTable.h
    bytecode/LLIntPrototypeLoadAdaptiveStructureWatchpoint.h
    bytecode/LazyOperandValueProfile.h
    bytecode/LazyValueProfile.h
    bytecode/LineColumn.h
    bytecode/LinkTimeConstant.h
    bytecode/MetadataTable.h
    bytecode/ObjectAllocationProfile.h
    bytecode/ObjectPropertyCondition.h
    bytecode/Opcode.h
    bytecode/OpcodeSize.h
    bytecode/Operands.h
    bytecode/PropertyCondition.h
    bytecode/PutByIdFlags.h
    bytecode/SetPrivateBrandStatus.h
    bytecode/SetPrivateBrandVariant.h
    bytecode/SourceID.h
    bytecode/SpeculatedType.h
    bytecode/StructureSet.h
    bytecode/SuperSampler.h
    bytecode/ToThisStatus.h
    bytecode/TypeLocation.h
    bytecode/UnlinkedCodeBlock.h
    bytecode/UnlinkedEvalCodeBlock.h
    bytecode/UnlinkedFunctionExecutable.h
    bytecode/UnlinkedGlobalCodeBlock.h
    bytecode/UnlinkedMetadataTable.h
    bytecode/ValueProfile.h
    bytecode/ValueRecovery.h
    bytecode/VariableWriteFireDetail.h
    bytecode/VirtualRegister.h
    bytecode/Watchpoint.h

    debugger/Breakpoint.h
    debugger/Debugger.h
    debugger/DebuggerCallFrame.h
    debugger/DebuggerParseData.h
    debugger/DebuggerPrimitives.h

    dfg/DFGCodeOriginPool.h
    dfg/DFGCommon.h
    dfg/DFGDoesGCCheck.h
    dfg/DFGMinifiedID.h

    domjit/DOMJITAbstractHeap.h
    domjit/DOMJITCallDOMGetterSnippet.h
    domjit/DOMJITEffect.h
    domjit/DOMJITGetterSetter.h
    domjit/DOMJITHeapRange.h
    domjit/DOMJITSignature.h

    heap/AbstractSlotVisitor.h
    heap/AbstractSlotVisitorInlines.h
    heap/AlignedMemoryAllocator.h
    heap/AllocationFailureMode.h
    heap/Allocator.h
    heap/AllocatorInlines.h
    heap/AllocatorForMode.h
    heap/BlockDirectory.h
    heap/BlockDirectoryBits.h
    heap/BlockDirectoryInlines.h
    heap/CellAttributes.h
    heap/CellContainer.h
    heap/CellContainerInlines.h
    heap/CellState.h
    heap/CollectionScope.h
    heap/CollectorPhase.h
    heap/CompleteSubspace.h
    heap/CompleteSubspaceInlines.h
    heap/ConstraintConcurrency.h
    heap/ConstraintParallelism.h
    heap/ConstraintVolatility.h
    heap/DeferGC.h
    heap/DeferGCInlines.h
    heap/DeleteAllCodeEffort.h
    heap/DestructionMode.h
    heap/EdenGCActivityCallback.h
    heap/FastMallocAlignedMemoryAllocator.h
    heap/FreeList.h
    heap/FreeListInlines.h
    heap/FullGCActivityCallback.h
    heap/GCActivityCallback.h
    heap/GCAssertions.h
    heap/GCConductor.h
    heap/GCDeferralContext.h
    heap/GCIncomingRefCounted.h
    heap/GCIncomingRefCountedInlines.h
    heap/GCIncomingRefCountedSet.h
    heap/GCLogging.h
    heap/GCMemoryOperations.h
    heap/GCOwnedDataScope.h
    heap/GCRequest.h
    heap/GCSegmentedArray.h
    heap/Handle.h
    heap/HandleBlock.h
    heap/HandleForward.h
    heap/HandleSet.h
    heap/HandleTypes.h
    heap/Heap.h
    heap/HeapAnalyzer.h
    heap/HeapCell.h
    heap/HeapCellInlines.h
    heap/HeapCellType.h
    heap/HeapFinalizerCallback.h
    heap/HeapInlines.h
    heap/HeapObserver.h
    heap/HeapSnapshotBuilder.h
    heap/HeapSubspaceTypes.h
    heap/IncrementalSweeper.h
    heap/IsoCellSet.h
    heap/IsoHeapCellType.h
    heap/IsoInlinedHeapCellType.h
    heap/IsoInlinedHeapCellTypeInlines.h
    heap/IsoMemoryAllocatorBase.h
    heap/IsoSubspace.h
    heap/IsoSubspaceInlines.h
    heap/IsoSubspacePerVM.h
    heap/LocalAllocator.h
    heap/LocalAllocatorInlines.h
    heap/MachineStackMarker.h
    heap/MarkStack.h
    heap/MarkedBlock.h
    heap/MarkedBlockInlines.h
    heap/MarkedBlockSet.h
    heap/MarkedSpace.h
    heap/MarkingConstraint.h
    heap/MarkingConstraintExecutorPair.h
    heap/MutatorState.h
    heap/PackedCellPtr.h
    heap/PreciseAllocation.h
    heap/RegisterState.h
    heap/RootMarkReason.h
    heap/RunningScope.h
    heap/SimpleMarkingConstraint.h
    heap/SlotVisitor.h
    heap/SlotVisitorInlines.h
    heap/SlotVisitorMacros.h
    heap/Strong.h
    heap/StrongForward.h
    heap/StrongInlines.h
    heap/StructureAlignedMemoryAllocator.h
    heap/Subspace.h
    heap/SubspaceInlines.h
    heap/Synchronousness.h
    heap/TinyBloomFilter.h
    heap/VerifierSlotVisitor.h
    heap/VisitRaceKey.h
    heap/Weak.h
    heap/WeakBlock.h
    heap/WeakHandleOwner.h
    heap/WeakImpl.h
    heap/WeakInlines.h
    heap/WeakSet.h
    heap/WeakSetInlines.h

    inspector/AsyncStackTrace.h
    inspector/ConsoleMessage.h
    inspector/ContentSearchUtilities.h
    inspector/IdentifiersFactory.h
    inspector/InjectedScript.h
    inspector/InjectedScriptBase.h
    inspector/InjectedScriptHost.h
    inspector/InjectedScriptManager.h
    inspector/InjectedScriptModule.h
    inspector/InspectorAgentBase.h
    inspector/InspectorAgentRegistry.h
    inspector/InspectorBackendDispatcher.h
    inspector/InspectorEnvironment.h
    inspector/InspectorFrontendChannel.h
    inspector/InspectorFrontendRouter.h
    inspector/InspectorProtocolTypes.h
    inspector/InspectorTarget.h
    inspector/PerGlobalObjectWrapperWorld.h
    inspector/ScriptArguments.h
    inspector/ScriptCallFrame.h
    inspector/ScriptCallStack.h
    inspector/ScriptCallStackFactory.h
    inspector/ScriptFunctionCall.h

    inspector/agents/InspectorAgent.h
    inspector/agents/InspectorAuditAgent.h
    inspector/agents/InspectorConsoleAgent.h
    inspector/agents/InspectorDebuggerAgent.h
    inspector/agents/InspectorHeapAgent.h
    inspector/agents/InspectorRuntimeAgent.h
    inspector/agents/InspectorScriptProfilerAgent.h
    inspector/agents/InspectorTargetAgent.h

    inspector/augmentable/AugmentableInspectorControllerClient.h

    inspector/remote/RemoteAutomationTarget.h
    inspector/remote/RemoteConnectionToTarget.h
    inspector/remote/RemoteControllableTarget.h
    inspector/remote/RemoteInspectionTarget.h
    inspector/remote/RemoteInspector.h

    interpreter/CLoopStack.h
    interpreter/CallFrame.h
    interpreter/CallFrameInlines.h
    interpreter/CalleeBits.h
    interpreter/EntryFrame.h
    interpreter/FrameTracers.h
    interpreter/Interpreter.h
    interpreter/Register.h
    interpreter/RegisterInlines.h
    interpreter/ShadowChicken.h
    interpreter/StackVisitor.h
    interpreter/VMEntryRecord.h

    jit/AssemblyHelpers.h
    jit/AssemblyHelpersSpoolers.h
    jit/BaselineJITCode.h
    jit/BaselineJITRegisters.h
    jit/CallFrameShuffleData.h
    jit/CCallHelpers.h
    jit/ExecutableAllocator.h
    jit/ExecutableMemoryHandle.h
    jit/FPRInfo.h
    jit/GCAwareJITStubRoutine.h
    jit/GPRInfo.h
    jit/JITAllocator.h
    jit/JITCode.h
    jit/JITCodeMap.h
    jit/JITCompilation.h
    jit/JITCompilationEffort.h
    jit/JITCompilationMode.h
    jit/JITMathICForwards.h
    jit/JITOperations.h
    jit/JITStubRoutine.h
    jit/JITThunks.h
    jit/OperationResult.h
    jit/PCToCodeOriginMap.h
    jit/PolymorphicCallStubRoutine.h
    jit/Reg.h
    jit/RegisterAtOffset.h
    jit/RegisterAtOffsetList.h
    jit/RegisterMap.h
    jit/RegisterSet.h
    jit/SIMDInfo.h
    jit/SIMDShuffle.h
    jit/ScratchRegisterAllocator.h
    jit/Snippet.h
    jit/SnippetParams.h
    jit/SnippetReg.h
    jit/SnippetSlowPathCalls.h
    jit/SpillRegistersMode.h
    jit/TagRegistersMode.h
    jit/ThunkGenerator.h
    jit/Width.h

    llint/LLIntOpcode.h
    llint/LLIntThunks.h

    parser/Lexer.h
    parser/ParserArena.h
    parser/ParserError.h
    parser/ParserModes.h
    parser/ParserTokens.h
    parser/ResultType.h
    parser/SourceCode.h
    parser/SourceProvider.h
    parser/SourceProviderCache.h
    parser/SourceProviderCacheItem.h
    parser/SourceTaintedOrigin.h
    parser/UnlinkedSourceCode.h
    parser/VariableEnvironment.h

    profiler/ProfilerBytecode.h
    profiler/ProfilerBytecodeSequence.h
    profiler/ProfilerBytecodes.h
    profiler/ProfilerCompilation.h
    profiler/ProfilerCompilationKind.h
    profiler/ProfilerCompiledBytecode.h
    profiler/ProfilerDatabase.h
    profiler/ProfilerDumper.h
    profiler/ProfilerEvent.h
    profiler/ProfilerExecutionCounter.h
    profiler/ProfilerJettisonReason.h
    profiler/ProfilerOSRExit.h
    profiler/ProfilerOSRExitSite.h
    profiler/ProfilerOrigin.h
    profiler/ProfilerOriginStack.h
    profiler/ProfilerProfiledBytecodes.h
    profiler/ProfilerUID.h

    runtime/AbstractModuleRecord.h
    runtime/ArgList.h
    runtime/ArgumentsMode.h
    runtime/ArityCheckMode.h
    runtime/ArrayConstructor.h
    runtime/ArrayBuffer.h
    runtime/ArrayBufferSharingMode.h
    runtime/ArrayBufferView.h
    runtime/ArrayConventions.h
    runtime/ArrayPrototype.h
    runtime/ArrayStorage.h
    runtime/ArrayStorageInlines.h
    runtime/AssertInvariants.h
    runtime/AsyncIteratorPrototype.h
    runtime/AuxiliaryBarrier.h
    runtime/AuxiliaryBarrierInlines.h
    runtime/BasicBlockLocation.h
    runtime/BatchedTransitionOptimizer.h
    runtime/BigInt64Array.h
    runtime/BigIntObject.h
    runtime/BigIntPrototype.h
    runtime/BigUint64Array.h
    runtime/BooleanObject.h
    runtime/BooleanPrototype.h
    runtime/BrandedStructure.h
    runtime/BufferMemoryHandle.h
    runtime/Butterfly.h
    runtime/ButterflyInlines.h
    runtime/BytecodeCacheError.h
    runtime/CachePayload.h
    runtime/CacheUpdate.h
    runtime/CacheableIdentifier.h
    runtime/CacheableIdentifierInlines.h
    runtime/CachedBytecode.h
    runtime/CachedTypes.h
    runtime/CagedBarrierPtr.h
    runtime/CallData.h
    runtime/CatchScope.h
    runtime/CellSize.h
    runtime/ClassInfo.h
    runtime/ClonedArguments.h
    runtime/CodeSpecializationKind.h
    runtime/CommonIdentifiers.h
    runtime/CompilationResult.h
    runtime/Completion.h
    runtime/Concurrency.h
    runtime/ConcurrentJSLock.h
    runtime/ConfigFile.h
    runtime/ConsoleClient.h
    runtime/ConsoleTypes.h
    runtime/ConstantMode.h
    runtime/ConstructAbility.h
    runtime/ConstructData.h
    runtime/ConstructorKind.h
    runtime/ControlFlowProfiler.h
    runtime/CustomGetterSetter.h
    runtime/DOMAnnotation.h
    runtime/DOMAttributeGetterSetter.h
    runtime/DataView.h
    runtime/DateInstance.h
    runtime/DateInstanceCache.h
    runtime/DeferTermination.h
    runtime/DeferredWorkTimer.h
    runtime/DefinePropertyAttributes.h
    runtime/DeletePropertySlot.h
    runtime/DirectArguments.h
    runtime/DirectArgumentsOffset.h
    runtime/DirectEvalExecutable.h
    runtime/DisallowScope.h
    runtime/DisallowVMEntry.h
    runtime/DumpContext.h
    runtime/ECMAMode.h
    runtime/EnsureStillAliveHere.h
    runtime/EnumerationMode.h
    runtime/Error.h
    runtime/ErrorHandlingScope.h
    runtime/ErrorInstance.h
    runtime/ErrorPrototype.h
    runtime/ErrorType.h
    runtime/EvalExecutable.h
    runtime/Exception.h
    runtime/ExceptionEventLocation.h
    runtime/ExceptionExpectation.h
    runtime/ExceptionHelpers.h
    runtime/ExceptionScope.h
    runtime/ExecutableBase.h
    runtime/ExecutableBaseInlines.h
    runtime/Float16Array.h
    runtime/Float32Array.h
    runtime/Float64Array.h
    runtime/Forward.h
    runtime/FunctionConstructor.h
    runtime/FunctionExecutable.h
    runtime/FunctionHasExecutedCache.h
    runtime/FunctionPrototype.h
    runtime/FunctionRareData.h
    runtime/FuzzerAgent.h
    runtime/Gate.h
    runtime/GenericArguments.h
    runtime/GenericOffset.h
    runtime/GenericTypedArrayView.h
    runtime/GenericTypedArrayViewInlines.h
    runtime/GetPutInfo.h
    runtime/GetterSetter.h
    runtime/GetVM.h
    runtime/GlobalExecutable.h
    runtime/GlobalObjectMethodTable.h
    runtime/HashMapHelper.h
    runtime/Identifier.h
    runtime/IdentifierInlines.h
    runtime/ImplementationVisibility.h
    runtime/ImportMap.h
    runtime/IndexingHeader.h
    runtime/IndexingHeaderInlines.h
    runtime/IndexingType.h
    runtime/InferredValue.h
    runtime/InitializeThreading.h
    runtime/InlineAttribute.h
    runtime/Int16Array.h
    runtime/Int32Array.h
    runtime/Int8Array.h
    runtime/InternalFunction.h
    runtime/Intrinsic.h
    runtime/IterationKind.h
    runtime/IteratorOperations.h
    runtime/JSArray.h
    runtime/JSArrayInlines.h
    runtime/JSArrayBuffer.h
    runtime/JSArrayBufferPrototype.h
    runtime/JSArrayBufferView.h
    runtime/JSArrayBufferViewInlines.h
    runtime/JSArrayIterator.h
    runtime/JSAsyncFromSyncIterator.h
    runtime/JSBigInt.h
    runtime/JSBoundFunction.h
    runtime/JSCBytecodeCacheVersion.h
    runtime/JSCConfig.h
    runtime/JSCInlines.h
    runtime/JSCJSValue.h
    runtime/JSCJSValueInlines.h
    runtime/JSCPtrTag.h
    runtime/JSCallee.h
    runtime/JSCalleeInlines.h
    runtime/JSCast.h
    runtime/JSCell.h
    runtime/JSCellInlines.h
    runtime/JSCustomGetterFunction.h
    runtime/JSCustomSetterFunction.h
    runtime/JSDataView.h
    runtime/JSDateMath.h
    runtime/JSDestructibleObject.h
    runtime/JSDestructibleObjectHeapCellType.h
    runtime/JSExportMacros.h
    runtime/JSFunction.h
    runtime/JSFunctionInlines.h
    runtime/JSGenerator.h
    runtime/JSGenericTypedArrayView.h
    runtime/JSGenericTypedArrayViewInlines.h
    runtime/JSGenericTypedArrayViewPrototype.h
    runtime/JSGenericTypedArrayViewPrototypeInlines.h
    runtime/JSGlobalLexicalEnvironment.h
    runtime/JSGlobalObject.h
    runtime/JSGlobalObjectFunctions.h
    runtime/JSGlobalObjectInlines.h
    runtime/JSGlobalProxy.h
    runtime/JSGlobalProxyInlines.h
    runtime/JSImmutableButterfly.h
    runtime/JSInternalFieldObjectImpl.h
    runtime/JSInternalPromise.h
    runtime/JSIterator.h
    runtime/JSIteratorConstructor.h
    runtime/JSIteratorHelper.h
    runtime/JSIteratorPrototype.h
    runtime/JSLexicalEnvironment.h
    runtime/JSLock.h
    runtime/JSMap.h
    runtime/JSMapInlines.h
    runtime/JSMapIterator.h
    runtime/JSMicrotask.h
    runtime/JSModuleLoader.h
    runtime/JSModuleRecord.h
    runtime/JSNativeStdFunction.h
    runtime/JSONAtomStringCache.h
    runtime/JSONObject.h
    runtime/JSObject.h
    runtime/JSObjectInlines.h
    runtime/JSPromise.h
    runtime/JSPromiseConstructor.h
    runtime/JSPropertyNameEnumerator.h
    runtime/JSRegExpStringIterator.h
    runtime/JSRemoteFunction.h
    runtime/JSRunLoopTimer.h
    runtime/JSScope.h
    runtime/JSScriptFetchParameters.h
    runtime/JSScriptFetcher.h
    runtime/JSSegmentedVariableObject.h
    runtime/JSSet.h
    runtime/JSSetInlines.h
    runtime/JSSetIterator.h
    runtime/JSSourceCode.h
    runtime/JSString.h
    runtime/JSStringInlines.h
    runtime/JSSymbolTableObject.h
    runtime/JSType.h
    runtime/JSTypeInfo.h
    runtime/JSTypedArrays.h
    runtime/JSWeakMap.h
    runtime/JSWeakMapInlines.h
    runtime/JSWithScope.h
    runtime/JSWrapForValidIterator.h
    runtime/JSWrapperObject.h
    runtime/KeyAtomStringCache.h
    runtime/KeyAtomStringCacheInlines.h
    runtime/LazyClassStructure.h
    runtime/LazyProperty.h
    runtime/LeafExecutable.h
    runtime/Lookup.h
    runtime/MapConstructor.h
    runtime/MatchResult.h
    runtime/MathCommon.h
    runtime/MegamorphicCache.h
    runtime/MemoryMode.h
    runtime/MemoryStatistics.h
    runtime/Microtask.h
    runtime/ModuleProgramExecutable.h
    runtime/NativeCallee.h
    runtime/NativeExecutable.h
    runtime/NativeFunction.h
    runtime/NullGetterFunction.h
    runtime/NullSetterFunction.h
    runtime/NumberObject.h
    runtime/NumberPrototype.h
    runtime/NumericStrings.h
    runtime/ObjectConstructor.h
    runtime/ObjectInitializationScope.h
    runtime/ObjectPrototype.h
    runtime/Operations.h
    runtime/Options.h
    runtime/OptionsList.h
    runtime/OrderedHashTable.h
    runtime/OrderedHashTableHelper.h
    runtime/PageCount.h
    runtime/ParseInt.h
    runtime/PrivateFieldPutKind.h
    runtime/PrivateName.h
    runtime/ProgramExecutable.h
    runtime/PropertyDescriptor.h
    runtime/PropertyName.h
    runtime/PropertyNameArray.h
    runtime/PropertyOffset.h
    runtime/PropertySlot.h
    runtime/PropertyStorage.h
    runtime/PropertyTable.h
    runtime/Protect.h
    runtime/PrototypeKey.h
    runtime/ProxyObject.h
    runtime/PureNaN.h
    runtime/PutDirectIndexMode.h
    runtime/PutPropertySlot.h
    runtime/RegExp.h
    runtime/RegExpCachedResult.h
    runtime/RegExpGlobalData.h
    runtime/RegExpKey.h
    runtime/RegExpObject.h
    runtime/RegExpStringIteratorPrototype.h
    runtime/RegExpSubstringGlobalAtomCache.h
    runtime/ResourceExhaustion.h
    runtime/RuntimeFlags.h
    runtime/RuntimeType.h
    runtime/SamplingProfiler.h
    runtime/ScopeOffset.h
    runtime/ScopedArguments.h
    runtime/ScopedArgumentsTable.h
    runtime/Scribble.h
    runtime/ScriptExecutable.h
    runtime/ScriptExecutableInlines.h
    runtime/ScriptFetchParameters.h
    runtime/ScriptFetcher.h
    runtime/ShadowRealmObject.h
    runtime/SideDataRepository.h
    runtime/SlowPathFunction.h
    runtime/SmallStrings.h
    runtime/SourceOrigin.h
    runtime/SparseArrayValueMap.h
    runtime/StableSort.h
    runtime/StackAlignment.h
    runtime/StackFrame.h
    runtime/StringObject.h
    runtime/StringPrototype.h
    runtime/StringReplaceCache.h
    runtime/StringSplitCache.h
    runtime/Structure.h
    runtime/StructureCache.h
    runtime/StructureChain.h
    runtime/StructureID.h
    runtime/StructureInlines.h
    runtime/StructureRareData.h
    runtime/StructureRareDataInlines.h
    runtime/StructureTransitionTable.h
    runtime/SubspaceAccess.h
    runtime/Symbol.h
    runtime/SymbolPrototype.h
    runtime/SymbolTable.h
    runtime/SymbolTableOrScopeDepth.h
    runtime/SyntheticModuleRecord.h
    runtime/TemplateObjectDescriptor.h
    runtime/TemporalNow.h
    runtime/TemporalObject.h
    runtime/TestRunnerUtils.h
    runtime/ThrowScope.h
    runtime/ToNativeFromValue.h
    runtime/TypeError.h
    runtime/TypeInfoBlob.h
    runtime/TypeSet.h
    runtime/TypedArrayAdaptersForwardDeclarations.h
    runtime/TypedArrayAdaptors.h
    runtime/TypedArrayController.h
    runtime/TypedArrayInlines.h
    runtime/TypedArrayType.h
    runtime/TypedArrays.h
    runtime/TypeofType.h
    runtime/UGPRPair.h
    runtime/Uint16Array.h
    runtime/Uint32Array.h
    runtime/Uint8Array.h
    runtime/Uint8ClampedArray.h
    runtime/VM.h
    runtime/VMEntryScope.h
    runtime/VMEntryScopeInlines.h
    runtime/VMInlines.h
    runtime/VMTraps.h
    runtime/VMTrapsInlines.h
    runtime/VarOffset.h
    runtime/Watchdog.h
    runtime/WeakGCHashTable.h
    runtime/WeakGCMap.h
    runtime/WeakGCMapInlines.h
    runtime/WeakGCSet.h
    runtime/WeakGCSetInlines.h
    runtime/WrapForValidIteratorPrototype.h
    runtime/WrapForValidIteratorPrototypeInlines.h
    runtime/WriteBarrier.h
    runtime/WriteBarrierInlines.h

    tools/Integrity.h
    tools/IntegrityInlines.h
    tools/VMInspector.h
    tools/VMInspectorInlines.h

    wasm/WasmBranchHints.h
    wasm/WasmCallee.h
    wasm/WasmCalleeGroup.h
    wasm/WasmCallingConvention.h
    wasm/WasmCapabilities.h
    wasm/WasmCompilationContext.h
    wasm/WasmCompilationMode.h
    wasm/WasmContext.h
    wasm/WasmCreationMode.h
    wasm/WasmExceptionType.h
    wasm/WasmFaultSignalHandler.h
    wasm/WasmFormat.h
    wasm/WasmFunctionCodeBlockGenerator.h
    wasm/WasmFunctionIPIntMetadataGenerator.h
    wasm/WasmHandlerInfo.h
    wasm/WasmIPIntTierUpCounter.h
    wasm/WasmIPIntGenerator.h
    wasm/WasmIndexOrName.h
    wasm/WasmJS.h
    wasm/WasmLLIntBuiltin.h
    wasm/WasmLLIntTierUpCounter.h
    wasm/WasmMemory.h
    wasm/WasmMemoryInformation.h
    wasm/WasmModule.h
    wasm/WasmModuleInformation.h
    wasm/WasmName.h
    wasm/WasmNameSection.h
    wasm/WasmOSREntryData.h
    wasm/WasmSIMDOpcodes.h
    wasm/WasmSections.h
    wasm/WasmTypeDefinition.h
    wasm/WasmTypeDefinitionInlines.h
    wasm/WasmStreamingCompiler.h
    wasm/WasmStreamingParser.h
    wasm/WasmTierUpCount.h
    wasm/WasmValueLocation.h

    wasm/js/JSWebAssembly.h
    wasm/js/JSWebAssemblyMemory.h
    wasm/js/JSWebAssemblyModule.h
    wasm/js/WebAssemblyModuleRecord.h


    yarr/RegularExpression.h
    yarr/Yarr.h
    yarr/YarrErrorCode.h
    yarr/YarrFlags.h
    yarr/YarrInterpreter.h
    yarr/YarrJIT.h
    yarr/YarrMatchingContextHolder.h
    yarr/YarrParser.h
    yarr/YarrPattern.h
    yarr/YarrUnicodeProperties.h
)

if (USE_INSPECTOR_SOCKET_SERVER)
    list(APPEND JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS
        API/JSRemoteInspectorServer.h
    )
endif ()

# GENERATOR 1-B: particular LUT creator (for 1 file only)
GENERATE_HASH_LUT(${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table ${JavaScriptCore_DERIVED_SOURCES_DIR}/Lexer.lut.h)

file(MAKE_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr)

#GENERATOR: "RegExpJitTables.h": tables used by Yarr
add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/RegExpJitTables.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/yarr/create_regex_tables
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/yarr/create_regex_tables ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/RegExpJitTables.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/RegExpJitTables.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/yarr/YarrPattern.cpp ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/RegExpJitTables.h)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/UnicodePatternTables.h
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/yarr/generateYarrUnicodePropertyTables.py
    DEPENDS ${JAVASCRIPTCORE_DIR}/yarr/hasher.py ${JAVASCRIPTCORE_DIR}/ucd/DerivedBinaryProperties.txt ${JAVASCRIPTCORE_DIR}/ucd/DerivedCoreProperties.txt ${JAVASCRIPTCORE_DIR}/ucd/DerivedNormalizationProps.txt ${JAVASCRIPTCORE_DIR}/ucd/PropList.txt ${JAVASCRIPTCORE_DIR}/ucd/PropertyAliases.txt ${JAVASCRIPTCORE_DIR}/ucd/PropertyValueAliases.txt ${JAVASCRIPTCORE_DIR}/ucd/ScriptExtensions.txt ${JAVASCRIPTCORE_DIR}/ucd/Scripts.txt ${JAVASCRIPTCORE_DIR}/ucd/UnicodeData.txt ${JAVASCRIPTCORE_DIR}/ucd/emoji-data.txt
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/yarr/generateYarrUnicodePropertyTables.py ${JAVASCRIPTCORE_DIR}/ucd ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/UnicodePatternTables.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/UnicodePatternTables.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/yarr/UnicodePatternTables.cpp ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/UnicodePatternTables.h)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/YarrCanonicalizeUnicode.cpp
    MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/yarr/generateYarrCanonicalizeUnicode
    DEPENDS ${JAVASCRIPTCORE_DIR}/ucd/CaseFolding.txt
    COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/yarr/generateYarrCanonicalizeUnicode ${JAVASCRIPTCORE_DIR}/ucd/CaseFolding.txt ${JavaScriptCore_DERIVED_SOURCES_DIR}/yarr/YarrCanonicalizeUnicode.cpp
    VERBATIM)

#GENERATOR: "KeywordLookup.h": keyword decision tree used by the lexer
add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/KeywordLookup.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/KeywordLookupGenerator.py ${CMAKE_CURRENT_SOURCE_DIR}/parser/Keywords.table > ${JavaScriptCore_DERIVED_SOURCES_DIR}/KeywordLookup.h
    VERBATIM)
list(APPEND JavaScriptCore_HEADERS ${JavaScriptCore_DERIVED_SOURCES_DIR}/KeywordLookup.h)
WEBKIT_ADD_SOURCE_DEPENDENCIES(${CMAKE_CURRENT_SOURCE_DIR}/parser/Lexer.cpp ${JavaScriptCore_DERIVED_SOURCES_DIR}/KeywordLookup.h)


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
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Animation.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/ApplicationCache.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Audit.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Browser.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/CPUProfiler.json
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
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/IndexedDB.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Inspector.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/LayerTree.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Memory.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Network.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Page.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Recording.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Runtime.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/ScriptProfiler.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Security.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/ServiceWorker.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Target.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Timeline.json
    ${JAVASCRIPTCORE_DIR}/inspector/protocol/Worker.json
)

add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/CombinedDomains.json
    MAIN_DEPENDENCY ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py
    DEPENDS ${JavaScriptCore_INSPECTOR_DOMAINS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py ${JavaScriptCore_INSPECTOR_DOMAINS} "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" > ${JavaScriptCore_DERIVED_SOURCES_DIR}/CombinedDomains.json
    VERBATIM)

# Inspector Backend Dispatchers, Frontend Dispatchers, Type Builders
file(MAKE_DIRECTORY ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector)
add_custom_command(
    OUTPUT ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendDispatchers.cpp
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendDispatchers.h
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorFrontendDispatchers.cpp
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorFrontendDispatchers.h
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorProtocolObjects.cpp
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorProtocolObjects.h
           ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js
    MAIN_DEPENDENCY ${JavaScriptCore_DERIVED_SOURCES_DIR}/CombinedDomains.json
    DEPENDS ${JavaScriptCore_INSPECTOR_PROTOCOL_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_INSPECTOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector" --framework JavaScriptCore ${JavaScriptCore_DERIVED_SOURCES_DIR}/CombinedDomains.json
    VERBATIM)

set(JavaScriptCore_INTERFACE_LIBRARIES JavaScriptCore)
set(JavaScriptCore_INTERFACE_INCLUDE_DIRECTORIES
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)
set(JavaScriptCore_INTERFACE_DEPENDENCIES
    JavaScriptCore_CopyHeaders
    JavaScriptCore_CopyPrivateHeaders
)

WEBKIT_FRAMEWORK_DECLARE(JavaScriptCore)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (COMPILER_IS_GCC_OR_CLANG)
    # Avoid using fused multiply-add instructions since this could give different results
    # for e.g. parseInt depending on the platform and compilation flags.
    if (WTF_CPU_LOONGARCH64)
        WEBKIT_ADD_TARGET_CXX_FLAGS(JavaScriptCore -ffp-contract=off)
    else ()
        WEBKIT_ADD_TARGET_CXX_FLAGS(JavaScriptCore -ffp-contract=off -fno-slp-vectorize)
    endif ()

    # On MIPS we have to explicitly disable madd4, since some versions of gcc still generate
    # such instructions despite -ffp-contract=off
    if (WTF_CPU_MIPS)
        WEBKIT_ADD_TARGET_CXX_FLAGS(JavaScriptCore -mno-madd4)
    endif ()
endif ()

WEBKIT_COPY_FILES(JavaScriptCore_CopyHeaders
    DESTINATION ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}/JavaScriptCore
    FILES ${JavaScriptCore_PUBLIC_FRAMEWORK_HEADERS}
    FLATTENED
)

WEBKIT_COPY_FILES(JavaScriptCore_CopyPrivateHeaders
    DESTINATION ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/JavaScriptCore
    FILES ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS}
    FLATTENED
)
add_dependencies(JavaScriptCore_CopyPrivateHeaders Bytecodes JSCBuiltins)
# JavaScriptCore_CopyPrivateHeaders needs to have a direct or indirect
# dependency of JavaScriptCore for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise,
# CombinedDomains.json will be generated in both projects.
if (NOT INTERNAL_BUILD)
    add_dependencies(JavaScriptCore_CopyPrivateHeaders JavaScriptCore)
endif ()

target_include_directories(LowLevelInterpreterLib
    PRIVATE "$<TARGET_PROPERTY:JavaScriptCore,INCLUDE_DIRECTORIES>")
add_dependencies(LowLevelInterpreterLib Bytecodes JSCBuiltins)
if (TARGET WTF_CopyHeaders)
    add_dependencies(LowLevelInterpreterLib WTF_CopyHeaders)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX AND GCC_OFFLINEASM_SOURCE_MAP)
    message(STATUS "Enabling asm postprocessing")

    set(LowLevelInterpreter_LAUNCHER "${RUBY_EXECUTABLE} ${JavaScriptCore_SCRIPTS_SOURCES_DIR}/postprocess-asm")
    get_target_property(PROP_RULE_LAUNCH_COMPILE LowLevelInterpreterLib RULE_LAUNCH_COMPILE)
    if (PROP_RULE_LAUNCH_COMPILE)
        set(LowLevelInterpreter_LAUNCHER "${LowLevelInterpreter_LAUNCHER} ${PROP_RULE_LAUNCH_COMPILE}")
    endif ()
    set_property(TARGET LowLevelInterpreterLib
        PROPERTY RULE_LAUNCH_COMPILE "${LowLevelInterpreter_LAUNCHER}")

    # Pass in the filename as a magic preprocessor directive, so that
    # the wrapper can accurately identify the source file. This is incompatible
    # with LTO.
    set_source_files_properties("llint/LowLevelInterpreter.cpp"
        PROPERTIES
        COMPILE_DEFINITIONS "POSTPROCESS_ASM=llint/LowLevelInterpreter.cpp"
        COMPILE_OPTIONS "-fno-lto")
endif ()

# When building JavaScriptCore as an object library, we need to make sure the
# lowlevelinterpreter lib objects get propogated.
if (${JavaScriptCore_LIBRARY_TYPE} STREQUAL "OBJECT")
    list(APPEND JavaScriptCore_PRIVATE_LIBRARIES $<TARGET_OBJECTS:LowLevelInterpreterLib>)
else ()
    list(APPEND JavaScriptCore_SOURCES $<TARGET_OBJECTS:LowLevelInterpreterLib>)
endif ()

WEBKIT_COMPUTE_SOURCES(JavaScriptCore)
WEBKIT_FRAMEWORK(JavaScriptCore)

target_precompile_headers(JavaScriptCore PRIVATE JavaScriptCorePrefix.h)

if (NOT "${PORT}" STREQUAL "Mac")
    if (${JavaScriptCore_LIBRARY_TYPE} STREQUAL "SHARED")
        WEBKIT_POPULATE_LIBRARY_VERSION(JAVASCRIPTCORE)
        set_target_properties(JavaScriptCore PROPERTIES VERSION ${JAVASCRIPTCORE_VERSION} SOVERSION ${JAVASCRIPTCORE_VERSION_MAJOR})
        install(TARGETS JavaScriptCore DESTINATION "${LIB_INSTALL_DIR}")
    endif ()
endif ()

# Force staging of shared scripts, even if they aren't directly used to build JavaScriptCore.

add_custom_target(JavaScriptCoreSharedScripts DEPENDS ${JavaScriptCore_SCRIPTS})
add_dependencies(JavaScriptCore JavaScriptCoreSharedScripts ${JavaScriptCore_EXTRA_DEPENDENCIES})

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    # JavaScriptCoreSharedScripts and JSCBuiltins targets need to have
    # a direct or indirect dependency for CMake Visual Studio
    # generator to eliminate duplicated custom commands. Otherwise,
    # JavaScriptCore_SCRIPTS will be generated in both projects.
    add_dependencies(JavaScriptCoreSharedScripts JSCBuiltins)
endif ()

if (ENABLE_JAVASCRIPT_SHELL)
  add_subdirectory(shell)
endif ()
