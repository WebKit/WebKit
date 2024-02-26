list(APPEND ImageDiff_SOURCES
    skia/PlatformImageSkia.cpp
)

if (NOT USE_SYSTEM_MALLOC)
    #
    # Skia is built to use bmalloc/fastMalloc, but ImageDiff should not
    # link to WTF. Adding the malloc allocator source will make the linker
    # skip the memory allocator symbols from the Skia static library, thus
    # avoiding the need to link to WTF.
    #
    list(APPEND ImageDiff_SOURCES
        ${THIRDPARTY_DIR}/skia/src/ports/SkMemory_malloc.cpp
    )

    if (NOT DEFINED CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
        check_cxx_compiler_flag(-Wno-unused-parameter CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
    endif ()
    if (CXX_COMPILER_SUPPORTS_-Wno-unused-parameter)
        set_property(
            SOURCE ${THIRDPARTY_DIR}/skia/src/ports/SkMemory_malloc.cpp
            APPEND PROPERTY COMPILE_OPTIONS -Wno-unused-parameter
        )
    endif ()
endif ()

list(APPEND ImageDiff_LIBRARIES
    Skia
)

list(APPEND ImageDiff_PRIVATE_DEFINITIONS USE_SKIA=1)
