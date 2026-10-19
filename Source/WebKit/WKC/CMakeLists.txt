cmake_minimum_required(VERSION 3.16)
project(WebKitWiiU CXX C)

set(WKC_DIR     ${CMAKE_SOURCE_DIR})
set(WEBCORE_DIR ${CMAKE_SOURCE_DIR}/../../WebCore)
set(WTF_DIR     ${CMAKE_SOURCE_DIR}/../../WTF)
set(JSC_DIR     ${CMAKE_SOURCE_DIR}/../../JavaScriptCore)
set(BMALLOC_DIR ${CMAKE_SOURCE_DIR}/../../bmalloc)

get_filename_component(SOURCE_DIR "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)

# The normal browser profile must include WebCore's DOM, HTML, CSS, loader and
# rendering sources; without them WKC can build a platform library but cannot
# display real webpages. Keep OFF available only for explicit bootstrap/diagnostic
# builds while the full port is being brought up.
option(WKC_ENABLE_FULL_WEBCORE "Compile the common WebCore sources required to render webpages on Wii U" ON)
if (WKC_ENABLE_FULL_WEBCORE)
    set(_WKC_WEB_AUDIO_DEFAULT ON)
    set(_WKC_INDEXED_DATABASE_DEFAULT ON)
    set(_WKC_GAMEPAD_DEFAULT ON)
else ()
    set(_WKC_WEB_AUDIO_DEFAULT OFF)
    set(_WKC_INDEXED_DATABASE_DEFAULT OFF)
    set(_WKC_GAMEPAD_DEFAULT OFF)
endif ()

# The WKCWebView port bridges Wii U GamePad/Remote events to the standards-based
# navigator.getGamepads() API. Enable it together with the full WebCore DOM;
# the bootstrap-only archive stays lean until it includes the DOM-side provider.
option(WKC_ENABLE_GAMEPAD "Expose the W3C Gamepad API through Wii U controller events" ${_WKC_GAMEPAD_DEFAULT})
if (WKC_ENABLE_GAMEPAD)
    set(_WKC_GAMEPAD_DEFINE 1)
else ()
    set(_WKC_GAMEPAD_DEFINE 0)
endif ()

# Keep WebAssembly opt-in until JavaScriptCore has a PowerPC register/ABI path.
# The upstream WasmValueLocation/WasmCallingConvention headers currently depend
# on JIT register types that are unavailable for Wii U's PowerPC CLoop build;
# enabling the feature breaks even otherwise unrelated JavaScriptCore TUs.
# build.yml may still generate the metadata headers without enabling the feature.
set(_WKC_WEBASSEMBLY_DEFAULT OFF)

option(WKC_ENABLE_WEB_AUDIO "Compile Web Audio against the WKC audio backend" ${_WKC_WEB_AUDIO_DEFAULT})
if (WKC_ENABLE_WEB_AUDIO)
    set(_WKC_WEB_AUDIO_DEFINE 1)
else ()
    set(_WKC_WEB_AUDIO_DEFINE 0)
endif ()

# WebAssembly uses the portable IPInt interpreter on Wii U; JIT tiers remain off.
option(WKC_ENABLE_WEBASSEMBLY "Compile WebAssembly with the JSC interpreter tier" ${_WKC_WEBASSEMBLY_DEFAULT})
if (WKC_ENABLE_WEBASSEMBLY)
    set(_WKC_WEBASSEMBLY_DEFINE 1)
else ()
    set(_WKC_WEBASSEMBLY_DEFINE 0)
endif ()

# IndexedDB uses WebCore's SQLite layer with the WKC-specific SQLite filesystem.
option(WKC_ENABLE_INDEXED_DATABASE "Compile IndexedDB using the WKC SQLite backend" ${_WKC_INDEXED_DATABASE_DEFAULT})
if (WKC_ENABLE_INDEXED_DATABASE)
    set(_WKC_INDEXED_DATABASE_DEFINE 1)
else ()
    set(_WKC_INDEXED_DATABASE_DEFINE 0)
endif ()

# Use WebKit's existing Web Inspector implementation (Inspector agents and the
# DevTools Protocol), not a WKC-authored debugger. The legacy ENABLE_JAVASCRIPT_DEBUGGER
# feature macro gates this port bridge; it does not implement a second debugger.
# The browser embedder supplies/connects the frontend and chooses the transport.
# WKC deliberately does not open an unauthenticated network listener.
option(WKC_ENABLE_WEB_INSPECTOR "Compile WebKit Web Inspector and its WKC bridge" ON)
if (WKC_ENABLE_WEB_INSPECTOR)
    set(_WKC_WEB_INSPECTOR_DEFINE 1)
else ()
    set(_WKC_WEB_INSPECTOR_DEFINE 0)
endif ()

# WebGL requires a modern GraphicsContextGL platform backend. The 4.3 WKC
# GraphicsContext3D implementation targets removed APIs and is not a valid
# replacement. Keep WebGL disabled until the current WKC backend exists.
option(WKC_ENABLE_WEBGL "Enable WebGL (requires Source/WebCore/platform/graphics/WKC/GraphicsContextGLWKC.cpp)" OFF)
if (WKC_ENABLE_WEBGL)
    if (NOT EXISTS "${WEBCORE_DIR}/platform/graphics/WKC/GraphicsContextGLWKC.cpp")
        message(FATAL_ERROR "WKC_ENABLE_WEBGL=ON, but the modern WKC GraphicsContextGLWKC.cpp backend is missing. Port a backend for current WebKit before enabling WebGL.")
    endif ()
    set(_WKC_WEBGL_DEFINE 1)
else ()
    set(_WKC_WEBGL_DEFINE 0)
endif ()

# Enable Web Notifications only in the full WebCore profile by default.
# WebCore owns the JavaScript API; WKC forwards permission and display operations
# to the embedder so the Wii U application can supply UI and delivery. This does
# not by itself add entries to Nintendo's official Notifications inbox.
if (WKC_ENABLE_FULL_WEBCORE)
    set(_WKC_NOTIFICATIONS_DEFAULT ON)
else ()
    set(_WKC_NOTIFICATIONS_DEFAULT OFF)
endif ()
option(WKC_ENABLE_NOTIFICATIONS "Compile the Web Notifications API and WKC embedder bridge" ${_WKC_NOTIFICATIONS_DEFAULT})
if (WKC_ENABLE_NOTIFICATIONS)
    set(_WKC_NOTIFICATIONS_DEFINE 1)
else ()
    set(_WKC_NOTIFICATIONS_DEFINE 0)
endif ()

# Enable WebRTC API support in the default full-WebCore Wii U profile.
# The WebRTC backend still needs final consumer-link and on-console runtime
# integration, so this option enables the build path rather than claiming RTC
# is already production-ready. Set WKC_ENABLE_WEBRTC_API=OFF to disable it.
set(_WKC_WEBRTC_API_DEFAULT ON)
option(WKC_ENABLE_WEBRTC_API "Compile WebRTC API support for Wii U" ${_WKC_WEBRTC_API_DEFAULT})
if (WKC_ENABLE_WEBRTC_API)
    set(_WKC_WEBRTC_API_DEFINE 1)
else ()
    set(_WKC_WEBRTC_API_DEFINE 0)
endif ()

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
    ${WTF_DIR}/wtf/wiiu
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
    -DENABLE_JAVASCRIPT_DEBUGGER=${_WKC_WEB_INSPECTOR_DEFINE}
    -DENABLE_WEBGL=${_WKC_WEBGL_DEFINE}
    -DENABLE_WEB_AUDIO=${_WKC_WEB_AUDIO_DEFINE}
    -DENABLE_MEDIA_STREAM=${_WKC_WEBRTC_API_DEFINE}
    -DENABLE_GAMEPAD=${_WKC_GAMEPAD_DEFINE}
    -DENABLE_WEB_RTC=${_WKC_WEBRTC_API_DEFINE}
    -DENABLE_NOTIFICATIONS=${_WKC_NOTIFICATIONS_DEFINE}
    -DENABLE_FULLSCREEN_API=1
    -DENABLE_INDEXED_DATABASE=${_WKC_INDEXED_DATABASE_DEFINE}
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
    -DENABLE_WEBASSEMBLY=${_WKC_WEBASSEMBLY_DEFINE}
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
# Compile the non-JIT engine. JIT/DFG/FTL/B3 and architecture-specific assembler/
# disassembler code remain excluded for Wii U's PowerPC CLoop build. The portable
# Inspector protocol and agent sources are selected from Sources.txt below when
# the debugger feature is enabled. Generated builtin/Yarr/bytecode files are
# copied into the working tree by the CI generation steps.
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

if (WKC_ENABLE_WEBASSEMBLY)
    # WebAssembly has a portable IPInt interpreter; architecture-specific JIT
    # tiers remain disabled for Wii U. Guarded JIT TUs compile empty when off.
    file(GLOB JSC_WASM_SOURCES
        ${JSC_DIR}/wasm/*.cpp
        ${JSC_DIR}/wasm/js/*.cpp
    )
    list(APPEND JSC_CORE_SOURCES ${JSC_WASM_SOURCES})
    include_directories("${CMAKE_BINARY_DIR}/wkc-generated/jsc")
endif ()
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

# JavaScript Inspector is portable enough to use with the CLoop interpreter,
# but its protocol/agent sources must be compiled explicitly in this standalone
# target. Read the canonical JSC source list instead of globbing inspector/
# (which would accidentally pick up the separate remote-inspector transport).
if (WKC_ENABLE_WEB_INSPECTOR)
    file(STRINGS "${JSC_DIR}/Sources.txt" _WKC_JSC_SOURCE_LINES)
    set(_WKC_JSC_INSPECTOR_SOURCES)
    set(_WKC_JSC_INSPECTOR_MISSING_SOURCES)
    foreach (_source_line IN LISTS _WKC_JSC_SOURCE_LINES)
        string(STRIP "${_source_line}" _source_entry)
        if (_source_entry STREQUAL "" OR _source_entry MATCHES "^//")
            continue()
        endif ()
        string(REGEX REPLACE "[ \\t]+@.*$" "" _source_entry "${_source_entry}")
        string(STRIP "${_source_entry}" _source_entry)
        if (NOT _source_entry MATCHES "^inspector/.*\\.cpp$")
            continue()
        endif ()

        if (EXISTS "${JSC_DIR}/${_source_entry}")
            list(APPEND _WKC_JSC_INSPECTOR_SOURCES "${JSC_DIR}/${_source_entry}")
        else ()
            # The protocol generator writes these derived C++ files into the
            # JavaScriptCore forwarding directory as part of the CI build.
            get_filename_component(_inspector_basename "${_source_entry}" NAME)
            set(_generated_inspector_source "${JSC_DIR}/JavaScriptCore/${_inspector_basename}")
            if (EXISTS "${_generated_inspector_source}")
                list(APPEND _WKC_JSC_INSPECTOR_SOURCES "${_generated_inspector_source}")
            else ()
                list(APPEND _WKC_JSC_INSPECTOR_MISSING_SOURCES "${_source_entry}")
            endif ()
        endif ()
    endforeach ()

    if (_WKC_JSC_INSPECTOR_MISSING_SOURCES)
        string(JOIN "\\n" _WKC_JSC_INSPECTOR_MISSING_REPORT ${_WKC_JSC_INSPECTOR_MISSING_SOURCES})
        message(FATAL_ERROR "Web Inspector is enabled, but generated/source files are missing:\\n${_WKC_JSC_INSPECTOR_MISSING_REPORT}")
    endif ()

    list(APPEND JSC_CORE_SOURCES ${_WKC_JSC_INSPECTOR_SOURCES})

    # These protocol-generated implementations are necessary for a functional
    # Inspector backend. Do not let the archive build successfully if only the
    # source-list-selected agents are present but the protocol dispatchers were
    # not generated or were omitted from the CMake target.
    foreach (_required_inspector_source IN ITEMS
        "InspectorBackendDispatchers.cpp"
        "InspectorFrontendDispatchers.cpp"
        "InspectorProtocolObjects.cpp")
        set(_required_inspector_path "${JSC_DIR}/JavaScriptCore/${_required_inspector_source}")
        if (NOT EXISTS "${_required_inspector_path}")
            message(FATAL_ERROR "Web Inspector is enabled but the generated Inspector source is missing: ${_required_inspector_path}")
        endif ()
        list(FIND JSC_CORE_SOURCES "${_required_inspector_path}" _required_inspector_source_index)
        if (_required_inspector_source_index EQUAL -1)
            message(FATAL_ERROR "Web Inspector is enabled but the Inspector source is not included in JSC_CORE_SOURCES: ${_required_inspector_path}")
        endif ()
    endforeach ()

    list(LENGTH _WKC_JSC_INSPECTOR_SOURCES _WKC_JSC_INSPECTOR_COUNT)
    if (_WKC_JSC_INSPECTOR_COUNT LESS 1)
        message(FATAL_ERROR "Web Inspector is enabled but no portable JavaScriptCore Inspector sources were selected")
    endif ()
    message(STATUS "WKC Inspector: ${_WKC_JSC_INSPECTOR_COUNT} portable inspector/agent sources selected")
endif ()

list(LENGTH JSC_CORE_SOURCES JSC_CORE_COUNT)
message(STATUS "JSC core: ${JSC_CORE_COUNT} source files selected (CLoop, no JIT)")

# Select the current WKC adapters explicitly. Do not recursively import the
# legacy WebKit_WiiU4.3 html/shadow/WKC, loader/archive/mhtml/WKC or page/WKC
# directories: they implement older class interfaces and in some cases duplicate
# definitions now owned by modern WebCore. See PORTING_STATUS.md before porting
# any of those files into this build.
file(GLOB_RECURSE WEBCORE_PLATFORM_SOURCES
    ${WEBCORE_DIR}/platform/WKC/*.cpp
    ${WEBCORE_DIR}/platform/network/curl/*.cpp
    ${WEBCORE_DIR}/platform/network/wkc/*.cpp
    ${WEBCORE_DIR}/platform/graphics/WKC/*.cpp
    ${WEBCORE_DIR}/platform/sql/WKC/*.cpp
    ${WEBCORE_DIR}/platform/text/WKC/*.cpp
)

# A full in-process page engine must compile the WKC ResourceHandle bridge.
# Modern WebKit's default ResourceHandle.cpp delegates networking to the
# separate NetworkProcess, which this single-process Wii U port does not run.
# Fail configuration rather than publishing an archive that can render local
# content but cannot fetch the top-level document and its subresources.
if (WKC_ENABLE_FULL_WEBCORE)
    set(_WKC_RESOURCE_HANDLE_SOURCE "${WEBCORE_DIR}/platform/WKC/ResourceHandleWKC.cpp")
    if (NOT EXISTS "${_WKC_RESOURCE_HANDLE_SOURCE}")
        message(FATAL_ERROR "Full WebCore is enabled but the in-process WKC network backend is missing: ${_WKC_RESOURCE_HANDLE_SOURCE}")
    endif ()
    list(FIND WEBCORE_PLATFORM_SOURCES "${_WKC_RESOURCE_HANDLE_SOURCE}" _WKC_RESOURCE_HANDLE_SOURCE_INDEX)
    if (_WKC_RESOURCE_HANDLE_SOURCE_INDEX EQUAL -1)
        message(FATAL_ERROR "The in-process WKC network backend exists but is not selected for libWebKitWKC.a")
    endif ()
endif ()

# GraphicsContext3DWKC.cpp is a legacy WebKit 4.3 adapter. It includes the
# removed GraphicsContext3D.h and cannot be used by this modern WebCore build.
# Do not compile the obsolete adapter when WebGL is disabled; WebGL may only be
# enabled by the separate current GraphicsContextGLWKC.cpp implementation.
list(REMOVE_ITEM WEBCORE_PLATFORM_SOURCES
    "${WEBCORE_DIR}/platform/graphics/WKC/GraphicsContext3DWKC.cpp"
)

file(GLOB_RECURSE WKC_WEBKIT_SOURCES
    ${WKC_DIR}/webkit/*.cpp
    ${WKC_DIR}/WebCoreSupport/*.cpp
    ${WKC_DIR}/helpers/*.cpp
    ${WKC_DIR}/accessor/*.cpp
)

# WKCWebView.cpp is the canonical address-bar implementation: it validates the
# final URL with WebCore's URL parser and routes ordinary text to Google Search.
# Exclude the provisional helper, which defines the same WKCWebView method with
# different search behavior; keeping both in the static archive risks ambiguous
# symbol resolution in the final Wave Browser link.
list(REMOVE_ITEM WKC_WEBKIT_SOURCES "${WKC_DIR}/webkit/WKCBrowserNavigation.cpp")

if (WKC_ENABLE_WEB_INSPECTOR)
    # InspectorBackendClientWKC is the boundary between WebCore's DevTools
    # Protocol implementation and the public ChromeClientIf message callbacks.
    # Its absence would leave the API looking enabled without a working bridge.
    set(_WKC_INSPECTOR_BACKEND_SOURCE "${WKC_DIR}/WebCoreSupport/InspectorBackendClientWKC.cpp")
    if (NOT EXISTS "${_WKC_INSPECTOR_BACKEND_SOURCE}")
        message(FATAL_ERROR "Web Inspector is enabled but the WKC Inspector bridge is missing: ${_WKC_INSPECTOR_BACKEND_SOURCE}")
    endif ()
    list(FIND WKC_WEBKIT_SOURCES "${_WKC_INSPECTOR_BACKEND_SOURCE}" _WKC_INSPECTOR_BACKEND_SOURCE_INDEX)
    if (_WKC_INSPECTOR_BACKEND_SOURCE_INDEX EQUAL -1)
        message(FATAL_ERROR "Web Inspector is enabled but the WKC Inspector bridge is not included in WKC_WEBKIT_SOURCES")
    endif ()
    message(STATUS "WKC Inspector bridge: ${_WKC_INSPECTOR_BACKEND_SOURCE}")
endif ()

# The default WKC target remains the known-good platform/engine bring-up set.
# The full source-list probe is opt-in while the Wii U port is being completed:
# it compiles all currently available common WebCore sources from upstream's
# Sources.txt, plus files generated by build.yml. This deliberately lives in WKC
# so the upstream source tree does not need a tracked modification.
set(WEBCORE_CORE_SOURCES)
if (WKC_ENABLE_FULL_WEBCORE)
    file(STRINGS "${WEBCORE_DIR}/Sources.txt" _WKC_WEBCORE_SOURCE_LINES)
    set(_WKC_WEBCORE_MISSING_SOURCES)
    foreach (_source_line IN LISTS _WKC_WEBCORE_SOURCE_LINES)
        string(STRIP "${_source_line}" _source_entry)
        if (_source_entry STREQUAL "" OR _source_entry MATCHES "^//")
            continue()
        endif ()

        # WebCore's source list annotates some TUs with @cost, @header, and
        # @no-unify-when metadata. Those annotations are for upstream's unified
        # build generator; a standalone CMake target only needs the source path.
        string(REGEX REPLACE "[ \t]+@.*$" "" _source_entry "${_source_entry}")
        string(STRIP "${_source_entry}" _source_entry)
        if (NOT _source_entry MATCHES "\\.cpp$")
            continue()
        endif ()

        if (EXISTS "${WEBCORE_DIR}/${_source_entry}")
            list(APPEND WEBCORE_CORE_SOURCES "${WEBCORE_DIR}/${_source_entry}")
        elseif (EXISTS "${WEBCORE_DIR}/WebCore/${_source_entry}")
            # Generated JSC DOM bindings are normally emitted into the derived
            # sources directory; build.yml's forwarding/copy steps put generated
            # copies in WebCore/WebCore when they are available.
            list(APPEND WEBCORE_CORE_SOURCES "${WEBCORE_DIR}/WebCore/${_source_entry}")
        else ()
            # Do not silently lose entries from Apple's canonical source list.
            # Record unresolved paths so engine bring-up can tell a deliberately
            # excluded platform feature from a missing generated source.
            list(APPEND _WKC_WEBCORE_MISSING_SOURCES "${_source_entry}")
        endif ()
    endforeach ()

    file(GLOB _WKC_GENERATED_WEBCORE_SOURCES "${WEBCORE_DIR}/WebCore/*.cpp")
    list(APPEND WEBCORE_CORE_SOURCES ${_WKC_GENERATED_WEBCORE_SOURCES})

    # ResourceHandle.cpp's platform implementation is replaced by the WKC
    # in-process bridge in platform/WKC/ResourceHandleWKC.cpp. Do not keep the
    # upstream TU in the full Sources.txt list: undefining USE_CURL on its
    # compile command is not reliable because config.h can define it again,
    # and compiling both implementations risks duplicate ResourceHandle symbols.
    list(REMOVE_ITEM WEBCORE_CORE_SOURCES
        "${WEBCORE_DIR}/platform/network/ResourceHandle.cpp"
    )
    list(REMOVE_ITEM WEBCORE_CORE_SOURCES ${WEBCORE_PLATFORM_SOURCES})
    list(REMOVE_DUPLICATES WEBCORE_CORE_SOURCES)
    list(LENGTH WEBCORE_CORE_SOURCES _WKC_WEBCORE_SOURCE_COUNT)
    message(STATUS "WKC full WebCore probe: ${_WKC_WEBCORE_SOURCE_COUNT} source files selected from Sources.txt/generated outputs")

    # Emit a complete omission report even while the full engine is still being
    # brought up. This prevents missing Sources.txt entries from going unnoticed.
    if (_WKC_WEBCORE_MISSING_SOURCES)
        list(LENGTH _WKC_WEBCORE_MISSING_SOURCES _WKC_WEBCORE_MISSING_COUNT)
        string(JOIN "\n" _WKC_WEBCORE_MISSING_REPORT ${_WKC_WEBCORE_MISSING_SOURCES})
        file(WRITE "${CMAKE_BINARY_DIR}/wkc-missing-webcore-sources.txt" "${_WKC_WEBCORE_MISSING_REPORT}\n")
        message(WARNING "WKC full WebCore omitted ${_WKC_WEBCORE_MISSING_COUNT} Sources.txt entries; full list: ${CMAKE_BINARY_DIR}/wkc-missing-webcore-sources.txt")
        set(_WKC_WEBCORE_MISSING_SAMPLE_COUNT 0)
        foreach (_missing_source IN LISTS _WKC_WEBCORE_MISSING_SOURCES)
            if (_WKC_WEBCORE_MISSING_SAMPLE_COUNT GREATER_EQUAL 40)
                break()
            endif ()
            message(STATUS "WKC missing WebCore source: ${_missing_source}")
            math(EXPR _WKC_WEBCORE_MISSING_SAMPLE_COUNT "${_WKC_WEBCORE_MISSING_SAMPLE_COUNT} + 1")
        endforeach ()
    else ()
        file(WRITE "${CMAKE_BINARY_DIR}/wkc-missing-webcore-sources.txt" "None\n")
        message(STATUS "WKC full WebCore resolved every .cpp entry in Sources.txt")
    endif ()
endif ()

# Full WebCore requires the shared platform-side audio and media-stream
# implementations in a# Optional audio and media-stream platform files must follow their feature
# switches. The page-loading baseline intentionally builds without Web Audio or
# WebRTC; compiling the legacy WKC audio/RTC adapters anyway drags removed APIs
# and the unported libwebrtc tree into an otherwise unrelated browser build.
# Re-enable these source groups only alongside their corresponding backend.
if (WKC_ENABLE_FULL_WEBCORE AND WKC_ENABLE_WEB_AUDIO)
    file(GLOB_RECURSE WEBCORE_AUDIO_WKC_SOURCES "${WEBCORE_DIR}/platform/audio/WKC/*.cpp")
    file(GLOB WEBCORE_AUDIO_COMMON_SOURCES "${WEBCORE_DIR}/platform/audio/*.cpp")
    list(APPEND WEBCORE_PLATFORM_SOURCES
        ${WEBCORE_AUDIO_WKC_SOURCES}
        ${WEBCORE_AUDIO_COMMON_SOURCES}
    )
endif ()
if (WKC_ENABLE_FULL_WEBCORE AND WKC_ENABLE_WEBRTC_API)
    file(GLOB_RECURSE WEBCORE_MEDIASTREAM_WKC_SOURCES "${WEBCORE_DIR}/platform/mediastream/WKC/*.cpp")
    file(GLOB WEBCORE_MEDIASTREAM_COMMON_SOURCES "${WEBCORE_DIR}/platform/mediastream/*.cpp")
    list(APPEND WEBCORE_PLATFORM_SOURCES
        ${WEBCORE_MEDIASTREAM_WKC_SOURCES}
        ${WEBCORE_MEDIASTREAM_COMMON_SOURCES}
    )
endif ()
if (WKC_ENABLE_FULL_WEBCORE)
    list(REMOVE_DUPLICATES WEBCORE_PLATFORM_SOURCES)
    list(LENGTH WEBCORE_PLATFORM_SOURCES _WKC_PLATFORM_SOURCE_COUNT)
    message(STATUS "WKC full WebCore probe: ${_WKC_PLATFORM_SOURCE_COUNT} WKC/common platform sources selected")
endif ()
selected WKC providers compile and the final executable links.
if (WKC_ENABLE_WEBRTC_API)
    add_definitions(-DUSE_LIBWEBRTC=1)

    # Give interface enumeration its own archive. It must be linked after
    # libwebrtc because the latter references getifaddrs/freeifaddrs.
    list(REMOVE_ITEM WKC_WEBKIT_SOURCES "${WKC_DIR}/webkit/WKCIfAddrs.cpp")

    list(APPEND WEBCORE_PLATFORM_SOURCES
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/LibWebRTCProvider.cpp
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/LibWebRTCAudioModule.cpp
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/LibWebRTCLogSink.cpp
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/LibWebRTCDTMFSenderBackend.cpp
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/LibWebRTCVideoFrameUtilities.cpp
        ${WEBCORE_DIR}/platform/mediastream/libwebrtc/VideoFrameLibWebRTC.cpp
    )

    # Reuse the Wii U-specific standalone wrapper so the engine and RTC library
    # share the same PPC toolchain, compatibility headers and temporary vendor
    # patches. This adds the real libwebrtc target to the same CMake graph.
    set(CMAKE_PROJECT_libwebrtc_wiiu_INCLUDE
        "${WKC_DIR}/cmake/WebRTCPlatform.cmake")
    set(LIBWEBRTC_DIR "${SOURCE_DIR}/ThirdParty/libwebrtc" CACHE PATH "" FORCE)
    add_subdirectory(
        "${WKC_DIR}/../../../Tools/wiiu/libwebrtc"
        "${CMAKE_BINARY_DIR}/wiiu-libwebrtc")
else ()
    add_definitions(-DUSE_LIBWEBRTC=0)
endif ()

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
    ${WTF_CORE_SOURCES}
    ${JSC_CORE_SOURCES}
    ${WEBCORE_PLATFORM_SOURCES}
    ${WKC_WEBKIT_SOURCES}
    ${WEBCORE_CORE_SOURCES}
    ${JSC_BUILTINS_SOURCES}
    ${WEBCORE_BUILTINS_SOURCES}
)

# The full in-process browser must not quietly compile without its embedded
# trust store: that would make HTTPS depend on a conventional filesystem CA
# bundle which Wii U installations do not necessarily have.
if (WKC_ENABLE_FULL_WEBCORE)
    target_compile_definitions(WebKitWKC PRIVATE WKC_REQUIRE_EMBEDDED_CA_BUNDLE=1)
endif ()

if (WKC_ENABLE_WEBRTC_API)
    add_library(WKCNetworkCompat STATIC "${WKC_DIR}/webkit/WKCIfAddrs.cpp")
    target_include_directories(WKCNetworkCompat PRIVATE
        "${WKC_DIR}/stubs"
        "${WKC_DIR}/include"
        "${WKC_DIR}/include/wkc"
        "${WEBCORE_DIR}"
        "${WEBCORE_DIR}/WebCore"
    )
endif ()

target_include_directories(WebKitWKC PUBLIC
    ${WKC_DIR}/include
    ${WKC_DIR}/include/wkc
    ${WKC_DIR}/webkit
)

if (WKC_ENABLE_WEBRTC_API)
    # WebKit's LibWebRTC provider includes libwebrtc and Abseil headers. Their
    # targets are built as a child project, so expose the source/generated
    # include roots explicitly to this static target.
    target_include_directories(WebKitWKC PRIVATE
        # WebCore's provider and wrapper headers are stored in the platform
        # directories, not all in the generated WebCore forwarding directory.
        "${WEBCORE_DIR}/platform/mediastream"
        "${WEBCORE_DIR}/platform/mediastream/libwebrtc"
        "${WEBCORE_DIR}/platform/audio"
        "${LIBWEBRTC_DIR}/Source"
        "${LIBWEBRTC_DIR}/Source/third_party/abseil-cpp"
        "${LIBWEBRTC_DIR}/Source/third_party/boringssl/src/include"
        "${CMAKE_BINARY_DIR}/libwebrtc/DerivedSources"
        "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders/libwebrtc"
    )

    # Propagate libwebrtc and its bundled dependency link requirements to the
    # final WKC consumer. The produced static archives still need a final link
    # test before RTC can be treated as production-ready.
    # WKCNetworkCompat comes after libwebrtc so its getifaddrs symbols
    # satisfy the unresolved references introduced by the RTC archive.
    target_link_libraries(WebKitWKC PUBLIC webrtc WKCNetworkCompat)
endif ()

install(TARGETS WebKitWKC DESTINATION lib)

# Export a consumer-ready SDK layout. Keep <wkc/...> headers at include/wkc,
# while WKCWebView/WKCClientBuilders and their helper headers are also required
# by embedders such as Wave Browser. Installing include/ wholesale under
# include/wkc would incorrectly produce include/wkc/wkc/wkcbase.h.
install(DIRECTORY ${WKC_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
    PATTERN "wkc" EXCLUDE
)
install(DIRECTORY ${WKC_DIR}/include/wkc/
    DESTINATION include/wkc
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY ${WKC_DIR}/webkit/
    DESTINATION include/wkc
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY ${WKC_DIR}/helpers/
    DESTINATION include/wkc/helpers
    FILES_MATCHING PATTERN "*.h"
)
