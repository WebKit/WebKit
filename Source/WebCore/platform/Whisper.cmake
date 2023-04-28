if (USE_WHISPER)
    set(WHISPER_DIR
        ${THIRDPARTY_DIR}/whisper.cpp
    )
    set(WHISPER_SOURCES
        ${WHISPER_DIR}/ggml.c
        ${WHISPER_DIR}/whisper.cpp
    )

    # FIXME: These flags are a temporary measure to suppress compilation issues
    # until the next release contains the following fix.
    # https://github.com/ggerganov/whisper.cpp/pull/1227
    set(WHISPER_C_FLAGS "-Wno-undef -Wno-array-bounds")
    # FIXME: the following options are intel CPU specific. They need to be tweaked
    # for other CPU architectures since they substantially affect performance.
    # Check ${THIRDPARTY_DIR}/whisper.cpp/Makefile
    if (WTF_CPU_X86_64)
        string(APPEND WHISPER_C_FLAGS " -mavx2 -mfma -mf16c -mavx -msse3")
    endif ()

    set_source_files_properties(
        ${WHISPER_SOURCES}
        PROPERTIES COMPILE_FLAGS "${WHISPER_C_FLAGS}"
    )
    add_library(whisper STATIC ${WHISPER_SOURCES})

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WHISPER_DIR}
    )
    list(APPEND WebCore_LIBRARIES
        whisper
    )
endif ()
