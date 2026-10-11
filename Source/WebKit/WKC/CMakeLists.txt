cmake_minimum_required(VERSION 3.16)
project(WebKitWiiU CXX C)

set(WKC_DIR     ${CMAKE_SOURCE_DIR})
set(WEBCORE_DIR ${CMAKE_SOURCE_DIR}/../../WebCore)
set(WTF_DIR     ${CMAKE_SOURCE_DIR}/../../WTF)
set(JSC_DIR     ${CMAKE_SOURCE_DIR}/../../JavaScriptCore)
set(BMALLOC_DIR ${CMAKE_SOURCE_DIR}/../../bmalloc)

get_filename_component(SOURCE_DIR "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

include_directories(
    ${WKC_DIR}/stubs
    ${SOURCE_DIR}
    ${JSC_DIR}
    ${JSC_DIR}/JavaScriptCore
    ${JSC_DIR}/runtime
    ${JSC_DIR}/bytecode
    ${JSC_DIR}/bytecompiler
    ${JSC_DIR}/heap
    ${JSC_DIR}/interpreter
    ${JSC_DIR}/llint
    ${JSC_DIR}/jit
    ${JSC_DIR}/assembler
    ${JSC_DIR}/parser
    ${JSC_DIR}/profiler
    ${JSC_DIR}/debugger
    ${JSC_DIR}/inspector
    ${JSC_DIR}/inspector/agents
    ${JSC_DIR}/dfg
    ${JSC_DIR}/ftl
    ${JSC_DIR}/b3
    ${JSC_DIR}/b3/air
    ${JSC_DIR}/wasm
    ${JSC_DIR}/wasm/js
    ${JSC_DIR}/yarr
    ${JSC_DIR}/API
    ${JSC_DIR}/domjit
    ${JSC_DIR}/tools
    ${JSC_DIR}/builtins
    ${JSC_DIR}/disassembler
    ${WEBCORE_DIR}
    ${WEBCORE_DIR}/WebCore
    ${WEBCORE_DIR}/PAL
    ${WEBCORE_DIR}/PAL/pal
    ${WEBCORE_DIR}/bindings
    ${WEBCORE_DIR}/bindings/js
    ${WEBCORE_DIR}/dom
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/fileapi
    ${WEBCORE_DIR}/html
    ${WEBCORE_DIR}/html/shadow
    ${WEBCORE_DIR}/loader
    ${WEBCORE_DIR}/loader/cache
    ${WEBCORE_DIR}/page
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/WKC
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/graphics/WKC
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/platform/network/curl
    ${WEBCORE_DIR}/platform/audio/WKC
    ${WEBCORE_DIR}/platform/text/WKC
    ${WEBCORE_DIR}/rendering
    ${WEBCORE_DIR}/style
    ${WEBCORE_DIR}/svg
    ${WTF_DIR}
    ${WTF_DIR}/wtf
    ${WTF_DIR}/wtf/WKC
    ${WTF_DIR}/wtf/text
    ${BMALLOC_DIR}
    /tmp/icu-headers
    /tmp/ssl-headers
    /tmp/curl-headers
    /tmp/sqlite-headers
    /tmp/psl-headers
    ${WKC_DIR}
    ${WKC_DIR}/include
    ${WKC_DIR}/include/wkc
    ${WKC_DIR}/webkit
    ${WKC_DIR}/WebCoreSupport
    ${WKC_DIR}/helpers
    ${WKC_DIR}/helpers/privates
    ${WKC_DIR}/accessor
    ${WKC_DIR}/accessor/privates
)

add_definitions(
    -DENABLE_WKC=1
    -DWTF_PLATFORM_WKC=1
    -DUSE_CURL=1
    -DUSE_FREETYPE=1
    -DWTF_USE_BMALLOC=0
    -DUSE_SYSTEM_MALLOC=1
    -DUSE_PTHREADS=1
    -DENABLE_JAVASCRIPT_DEBUGGER=0
    -DENABLE_WEBGL=1
    -DENABLE_WEB_AUDIO=0
    -DENABLE_MEDIA_STREAM=1
    -DENABLE_WEB_RTC=0
    -DENABLE_NOTIFICATIONS=0
    -DENABLE_FULLSCREEN_API=0
    -DENABLE_INDEXED_DATABASE=0
    -DENABLE_SVG=1
    -DENABLE_CSS_ANIMATIONS=1
    -DENABLE_WEB_ARCHIVE=1
    -DENABLE_MHTML=1
    -DENABLE_XSLT=0
    # --- JavaScriptCore: interpreter (CLoop) tier, no JIT (Wii U PowerPC) ---
    -DENABLE_C_LOOP=1
    -DENABLE_JIT=0
    -DENABLE_DFG_JIT=0
    -DENABLE_FTL_JIT=0
    -DENABLE_WEBASSEMBLY=0
    -DENABLE_WEBASSEMBLY_BBQJIT=0
    -DENABLE_WEBASSEMBLY_OMGJIT=0
    -DENABLE_SAMPLING_PROFILER=0
    "-DWEBCORE_EXPORT="
    "-DPAL_EXPORT="
    "-DWTF_EXPORT_PRIVATE="
    "-DJSEXPORT="
    "-DJS_EXPORT_PRIVATE="
    -DWKC_CUSTOMER_WIIU
)

add_compile_options(
    -include ${WKC_DIR}/stubs/WKCPlatformStubs.h
    -Wno-sfinae-incomplete
    # PowerPC 750 has no AltiVec, so GCC emulates the SIMD vector types WTF uses
    # (e.g. BitVector's SIMD::isNonZero fast path) and warns that passing such a
    # vector by reference is a non-standard ABI extension. It is harmless here --
    # the whole library is built with one toolchain, so there is no cross-GCC ABI
    # mixing -- and it is the conventional flag to silence on PPC/ARM targets.
    -Wno-psabi
)

file(GLOB_RECURSE WTF_WKC_SOURCES
    ${WTF_DIR}/wtf/WKC/*.cpp
)

# ---------------------------------------------------------------------------
# Engine bring-up, step 1: WTF core (the base library).
# ---------------------------------------------------------------------------
file(GLOB WTF_CORE_SOURCES
    ${WTF_DIR}/wtf/*.cpp
    ${WTF_DIR}/wtf/text/*.cpp
    ${WTF_DIR}/wtf/dtoa/*.cpp
    ${WTF_DIR}/wtf/threads/*.cpp
    ${WTF_DIR}/wtf/persistence/*.cpp
    ${WTF_DIR}/wtf/unicode/*.cpp
    ${WTF_DIR}/wtf/unicode/icu/*.cpp
    ${WTF_DIR}/wtf/generic/*.cpp
    ${WTF_DIR}/wtf/wiiu/*.cpp
)
# NOTE: CollatorWKC.cpp and UnicodeWKC.cpp/.h were deleted -- they implemented the
# 2014 WTF::Collator / peer-based WTF::Unicode interfaces that modern WebKit removed.
# The modern equivalents are provided upstream (wtf/unicode/CollatorDefault.cpp and
# wtf/unicode/icu/*), which are already compiled here.
#
# Upstream WTF files with no Wii U platform branch are replaced by Wii U backends
# in wtf/wiiu/ (globbed above). Drop the upstream originals so their #error /
# missing-POSIX code paths never compile:
#  - StackBounds.cpp             -> wiiu/StackBoundsWKC.cpp  (peer stack query;
#                                   upstream ends in "#error Need a way to get the
#                                   stack bounds" and OS(UNIX) needs pthread_attr_getstack)
#  - PageBlock.cpp               -> wiiu/PageBlockWKC.cpp    (systemPageSize() is
#                                   only defined for OS(UNIX)/OS(WINDOWS))
#  - generic/MemoryFootprintGeneric.cpp -> wiiu/MemoryFootprintWKC.cpp (memoryStatus()
#                                   is unimplemented here; use the WKC heap peer)
#  - RandomDevice.cpp + OSRandomSource.cpp -> wiiu/OSRandomSourceWKC.cpp (RandomDevice.h
#                                   itself #errors without a strong randomness source;
#                                   we fill from wkcRandomNumbersPeer instead)
list(REMOVE_ITEM WTF_CORE_SOURCES
    ${WTF_DIR}/wtf/StackBounds.cpp
    ${WTF_DIR}/wtf/PageBlock.cpp
    ${WTF_DIR}/wtf/generic/MemoryFootprintGeneric.cpp
    ${WTF_DIR}/wtf/RandomDevice.cpp
    ${WTF_DIR}/wtf/OSRandomSource.cpp
    # SIMDUTF.cpp pulls the vendored simdutf amalgamation, which declares it does
    # not support 32-bit platforms; on 32-bit big-endian PowerPC its scalar
    # fallback fails to compile. wtf/wiiu/SIMDUTFWKC.cpp provides a real scalar
    # backend behind the slim stubs/wtf/simdutf/simdutf_impl.h header instead.
    ${WTF_DIR}/wtf/SIMDUTF.cpp
)
#
# Threading strategy: the capability probe proved devkitPPC/WUT lacks POSIX
# semaphores (sem_t) and siginfo signals (sa_sigaction), so upstream
# ThreadingPOSIX.cpp cannot do the GC thread-suspend. Instead we keep the
# WKC-authored wtf/wiiu/ThreadingWKC.cpp (globbed above) and modernize it to the
# current Thread class on top of the wkcThread*Peer layer -- which already exposes
# thread suspend/resume and register capture (wkcThreadSuspendPeer /
# wkcThreadStoreThreadRegistersPeer / wkcThreadGetStackPointerPeer), purpose-built
# by ACCESS for GC stack scanning.
list(LENGTH WTF_CORE_SOURCES WTF_CORE_COUNT)
message(STATUS "WTF core: ${WTF_CORE_COUNT} source files selected (agnostic + generic + wiiu)")

# wtf/SIMDUTF.cpp textually includes the vendored simdutf amalgamation
# (wtf/simdutf/simdutf_impl.cpp.h). That third-party code is written against
# char16_t, but devkitPPC's GCC treats char16_t and uint16_t as distinct types
# and rejects the (ABI-identical) pointer conversions as errors rather than
# warnings. We must not edit the vendored amalgamation, so relax just this one
# translation unit with -fpermissive; every other file keeps strict diagnostics.
set_source_files_properties(${WTF_DIR}/wtf/SIMDUTF.cpp PROPERTIES COMPILE_OPTIONS "-fpermissive")

# ---------------------------------------------------------------------------
# Engine bring-up, step 2: JavaScriptCore core (interpreter/CLoop tier).
# Compile the non-JIT engine. The jit/dfg/ftl/b3/wasm/assembler/disassembler/
# inspector dirs are intentionally NOT compiled (x86/ARM-only, or need
# generated dispatchers); their headers remain on the include path above for
# code guarded by #if ENABLE(...). Generated sources (YarrCanonicalizeUnicode,
# BytecodeDumperGenerated) live in the generated JSC dir after the CI copy.
# ---------------------------------------------------------------------------
file(GLOB JSC_CORE_SOURCES
    ${JSC_DIR}/runtime/*.cpp
    ${JSC_DIR}/bytecode/*.cpp
    ${JSC_DIR}/bytecompiler/*.cpp
    ${JSC_DIR}/parser/*.cpp
    ${JSC_DIR}/heap/*.cpp
    ${JSC_DIR}/interpreter/*.cpp
    ${JSC_DIR}/llint/*.cpp
    ${JSC_DIR}/yarr/*.cpp
    ${JSC_DIR}/debugger/*.cpp
    ${JSC_DIR}/profiler/*.cpp
    ${JSC_DIR}/domjit/*.cpp
    ${JSC_DIR}/builtins/*.cpp
    ${JSC_DIR}/API/*.cpp
    ${JSC_DIR}/tools/*.cpp
    ${JSC_DIR}/JavaScriptCore/YarrCanonicalizeUnicode.cpp
    ${JSC_DIR}/JavaScriptCore/BytecodeDumperGenerated.cpp
)
# Drop upstream files the globs pick up that must NOT be in a Wii U library:
#  - JSStringRefBSTR.cpp  : Windows BSTR API
#  - JSStringRefCF.cpp    : Apple CoreFoundation API
#  - LLIntOffsetsExtractor / LLIntSettingsExtractor : build-time TOOLS with their
#    own main(); they generate offsets, they are not engine code.
list(REMOVE_ITEM JSC_CORE_SOURCES
    ${JSC_DIR}/API/JSStringRefBSTR.cpp
    ${JSC_DIR}/API/JSStringRefCF.cpp
    ${JSC_DIR}/llint/LLIntOffsetsExtractor.cpp
    ${JSC_DIR}/llint/LLIntSettingsExtractor.cpp
)
list(LENGTH JSC_CORE_SOURCES JSC_CORE_COUNT)
message(STATUS "JSC core: ${JSC_CORE_COUNT} source files selected (CLoop, no JIT)")

file(GLOB_RECURSE WEBCORE_PLATFORM_SOURCES
    ${WEBCORE_DIR}/platform/WKC/*.cpp
    ${WEBCORE_DIR}/platform/network/curl/*.cpp
    ${WEBCORE_DIR}/platform/graphics/WKC/*.cpp
    ${WEBCORE_DIR}/platform/audio/WKC/*.cpp
    ${WEBCORE_DIR}/platform/text/WKC/*.cpp
    ${WEBCORE_DIR}/platform/mediastream/WKC/*.cpp
)

file(GLOB_RECURSE WKC_WEBKIT_SOURCES
    ${WKC_DIR}/webkit/*.cpp
    ${WKC_DIR}/WebCoreSupport/*.cpp
    ${WKC_DIR}/helpers/*.cpp
    ${WKC_DIR}/accessor/*.cpp
)

list(REMOVE_ITEM WEBCORE_PLATFORM_SOURCES
    ${WEBCORE_DIR}/platform/WKC/ClipboardWKC.cpp
)

# JSC builtins — per-builtin .cpp files generated from .js sources
file(GLOB JSC_BUILTINS_SOURCES ${JSC_DIR}/JavaScriptCore/*Builtins.cpp)
if(JSC_BUILTINS_SOURCES)
    list(LENGTH JSC_BUILTINS_SOURCES JSC_BUILTINS_COUNT)
    message(STATUS "Found ${JSC_BUILTINS_COUNT} JSC builtin files — JS builtins will be compiled into engine")
else()
    message(WARNING "No JSC builtin .cpp files found — JS builtins (Promise, Array.map etc.) will not work")
endif()

# WebCore builtins (Fetch, Streams, etc.)
file(GLOB WEBCORE_BUILTINS_SOURCES
    ${JSC_DIR}/JavaScriptCore/WebCore*.cpp
)

add_library(WebKitWKC STATIC
    ${WTF_WKC_SOURCES}
    ${WTF_CORE_SOURCES}
    ${JSC_CORE_SOURCES}
    ${WEBCORE_PLATFORM_SOURCES}
    ${WKC_WEBKIT_SOURCES}
    ${JSC_BUILTINS_SOURCES}
    ${WEBCORE_BUILTINS_SOURCES}
)

target_include_directories(WebKitWKC PUBLIC
    ${WKC_DIR}/include
    ${WKC_DIR}/include/wkc
    ${WKC_DIR}/webkit
)

install(TARGETS WebKitWKC DESTINATION lib)
install(DIRECTORY ${WKC_DIR}/include/ DESTINATION include/wkc)
