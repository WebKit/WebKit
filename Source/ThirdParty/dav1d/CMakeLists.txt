# Mirrors Source/WebCore/PAL/ThirdParty/dav1d/dav1d.xcodeproj.
# config.h sets HAVE_ASM=1 only on arm64; on every other arch there are no .S/.asm files to build.

set(DAV1D_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore/PAL/ThirdParty/dav1d")

set(dav1d_generic_SOURCES
    src/cdf.c
    src/cpu.c
    src/ctx.c
    src/data.c
    src/decode.c
    src/dequant_tables.c
    src/getbits.c
    src/intra_edge.c
    src/itx_1d.c
    src/lf_mask.c
    src/lib.c
    src/log.c
    src/mem.c
    src/msac.c
    src/obu.c
    src/pal.c
    src/picture.c
    src/qm.c
    src/ref.c
    src/refmvs.c
    src/scan.c
    src/tables.c
    src/thread_task.c
    src/warpmv.c
    src/wedge.c
)

set(dav1d_tmpl_SOURCES
    src/cdef_apply_tmpl.c
    src/cdef_tmpl.c
    src/fg_apply_tmpl.c
    src/filmgrain_tmpl.c
    src/ipred_prepare_tmpl.c
    src/ipred_tmpl.c
    src/itx_tmpl.c
    src/lf_apply_tmpl.c
    src/loopfilter_tmpl.c
    src/looprestoration_tmpl.c
    src/lr_apply_tmpl.c
    src/mc_tmpl.c
    src/recon_tmpl.c
)

# arm64 assembly. The per-bitdepth arm init functions are static-inline in the arm/*.h headers, so
# arm/cpu.c is the only extra C file. The .S files are compiled once (no BITDEPTH) and set their own
# architecture via `.arch`, so no assembler flags are needed; the set matches src/meson.build.
if (WTF_CPU_ARM64)
    enable_language(ASM)
    list(APPEND dav1d_generic_SOURCES src/arm/cpu.c)
    set(dav1d_asm_SOURCES
        src/arm/64/cdef.S
        src/arm/64/cdef16.S
        src/arm/64/filmgrain.S
        src/arm/64/filmgrain16.S
        src/arm/64/ipred.S
        src/arm/64/ipred16.S
        src/arm/64/itx.S
        src/arm/64/itx16.S
        src/arm/64/loopfilter.S
        src/arm/64/loopfilter16.S
        src/arm/64/looprestoration.S
        src/arm/64/looprestoration16.S
        src/arm/64/looprestoration_common.S
        src/arm/64/mc.S
        src/arm/64/mc16.S
        src/arm/64/mc16_sve.S
        src/arm/64/mc_dotprod.S
        src/arm/64/msac.S
        src/arm/64/refmvs.S
    )
    list(TRANSFORM dav1d_asm_SOURCES PREPEND "${DAV1D_DIR}/")
endif ()

list(TRANSFORM dav1d_generic_SOURCES PREPEND "${DAV1D_DIR}/")
list(TRANSFORM dav1d_tmpl_SOURCES    PREPEND "${DAV1D_DIR}/")

add_library(dav1d_generic     OBJECT ${dav1d_generic_SOURCES})
add_library(dav1d_bitdepth_8  OBJECT ${dav1d_tmpl_SOURCES})
add_library(dav1d_bitdepth_16 OBJECT ${dav1d_tmpl_SOURCES})

target_compile_definitions(dav1d_bitdepth_8  PRIVATE BITDEPTH=8)
target_compile_definitions(dav1d_bitdepth_16 PRIVATE BITDEPTH=16)

set(dav1d_OBJECT_LIBRARIES dav1d_generic dav1d_bitdepth_8 dav1d_bitdepth_16)

if (WTF_CPU_ARM64)
    add_library(dav1d_asm OBJECT ${dav1d_asm_SOURCES})
    list(APPEND dav1d_OBJECT_LIBRARIES dav1d_asm)
endif ()

foreach (_t ${dav1d_OBJECT_LIBRARIES})
    target_include_directories(${_t} PRIVATE
        ${DAV1D_DIR}            # for #include "config.h" and "src/*.h"
        ${DAV1D_DIR}/include    # for #include <dav1d/dav1d.h>
    )
    target_compile_options(${_t} PRIVATE -w)  # third-party: suppress all warnings
    set_target_properties(${_t} PROPERTIES C_VISIBILITY_PRESET hidden)
endforeach ()
unset(_t)

add_library(dav1d STATIC)
foreach (_t ${dav1d_OBJECT_LIBRARIES})
    target_sources(dav1d PRIVATE $<TARGET_OBJECTS:${_t}>)
endforeach ()
unset(_t)
target_include_directories(dav1d INTERFACE ${DAV1D_DIR}/include)
set_target_properties(dav1d PROPERTIES C_VISIBILITY_PRESET hidden)
