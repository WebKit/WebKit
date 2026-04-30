# Mirrors Source/WebCore/PAL/ThirdParty/dav1d/dav1d.xcodeproj.
# Pre-committed config.h pins HAVE_ASM=0, so no .S/.asm files and no nasm/yasm/meson are needed.
# Three OBJECT libs (generic + _8bpc + _16bpc) merge into one STATIC dav1d, matching the Xcode
# libtool-merge of libdav1d_bitdepth_8.a + libdav1d_bitdepth_16.a + generic objects into libdav1d.a.

set(DAV1D_DIR "${CMAKE_SOURCE_DIR}/Source/WebCore/PAL/ThirdParty/dav1d")

set(dav1d_generic_SOURCES
    src/cdf.c
    src/cpu.c
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

list(TRANSFORM dav1d_generic_SOURCES PREPEND "${DAV1D_DIR}/")
list(TRANSFORM dav1d_tmpl_SOURCES    PREPEND "${DAV1D_DIR}/")

add_library(dav1d_generic     OBJECT ${dav1d_generic_SOURCES})
add_library(dav1d_bitdepth_8  OBJECT ${dav1d_tmpl_SOURCES})
add_library(dav1d_bitdepth_16 OBJECT ${dav1d_tmpl_SOURCES})

target_compile_definitions(dav1d_bitdepth_8  PRIVATE BITDEPTH=8)
target_compile_definitions(dav1d_bitdepth_16 PRIVATE BITDEPTH=16)

foreach (_t dav1d_generic dav1d_bitdepth_8 dav1d_bitdepth_16)
    target_include_directories(${_t} PRIVATE
        ${DAV1D_DIR}            # for #include "config.h" and "src/*.h"
        ${DAV1D_DIR}/include    # for #include <dav1d/dav1d.h>
    )
    target_compile_options(${_t} PRIVATE -w)  # third-party: suppress all warnings
    set_target_properties(${_t} PROPERTIES C_VISIBILITY_PRESET hidden)
endforeach ()
unset(_t)

add_library(dav1d STATIC
    $<TARGET_OBJECTS:dav1d_generic>
    $<TARGET_OBJECTS:dav1d_bitdepth_8>
    $<TARGET_OBJECTS:dav1d_bitdepth_16>
)
target_include_directories(dav1d INTERFACE ${DAV1D_DIR}/include)
set_target_properties(dav1d PROPERTIES C_VISIBILITY_PRESET hidden)
