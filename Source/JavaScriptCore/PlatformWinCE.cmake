if (ENABLE_JIT AND WTF_CPU_ARM)
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
        MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/create_jit_stubs
        DEPENDS ${JAVASCRIPTCORE_DIR}/jit/JITStubs.cpp
        COMMAND ${PERL_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/create_jit_stubs --prefix=MSVC ${JAVASCRIPTCORE_DIR}/jit/JITStubs.cpp > ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
        VERBATIM)

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj
        MAIN_DEPENDENCY ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm
        COMMAND armasm -nologo ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.asm ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj
        VERBATIM)

    list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_DIR}/GeneratedJITStubs.obj)
endif ()
