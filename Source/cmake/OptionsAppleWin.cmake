# The static runtime used to be required for AppleWin due to
# WebKitSystemInterface.lib being compiled with a static runtime. That library
# is no longer used, but we keep building with static runtime for backward
# compatibility. But if someone decides that it's OK to require existing
# projects to build with the runtime DLLs, that's now technically possible.
if (NOT DEBUG_SUFFIX)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

if (DEFINED ENV{AppleApplicationSupportSDK})
    file(TO_CMAKE_PATH "$ENV{AppleApplicationSupportSDK}/AppleInternal" WEBKIT_LIBRARIES_DIR)
    set(WEBKIT_LIBRARIES_INCLUDE_DIR "${WEBKIT_LIBRARIES_DIR}/include")
    include_directories(${WEBKIT_LIBRARIES_INCLUDE_DIR})
    set(APPLE_BUILD 1)
endif ()

if (NOT WEBKIT_LIBRARIES_DIR)
    if (DEFINED ENV{WEBKIT_LIBRARIES})
        file(TO_CMAKE_PATH "$ENV{WEBKIT_LIBRARIES}" WEBKIT_LIBRARIES_DIR)
    else ()
        file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/WebKitLibraries/win" WEBKIT_LIBRARIES_DIR)
    endif ()
endif ()

set(WTF_PLATFORM_APPLE_WIN ON)

include(OptionsWin)

set(ENABLE_WEBCORE ON)
set(ENABLE_WEBINSPECTORUI OFF)

SET_AND_EXPOSE_TO_BUILD(USE_CF ON)
SET_AND_EXPOSE_TO_BUILD(USE_CFURLCONNECTION ON)

set(SQLite3_NAMES SQLite3${DEBUG_SUFFIX})

find_package(Apple REQUIRED COMPONENTS
    AVFoundationCF
    ApplicationServices
    CFNetwork
    CoreFoundation
    CoreGraphics
    CoreText
    QuartzCore
    libdispatch
)

find_package(ICU 61.2 REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 REQUIRED)
find_package(LibXslt REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)

SET_AND_EXPOSE_TO_BUILD(USE_CA ON)
SET_AND_EXPOSE_TO_BUILD(USE_CG ON)
SET_AND_EXPOSE_TO_BUILD(USE_CORE_TEXT ON)

set(CMAKE_REQUIRED_INCLUDES ${WEBKIT_LIBRARIES_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES
    "${WEBKIT_LIBRARIES_LINK_DIR}/CoreFoundation${DEBUG_SUFFIX}.lib"
    "${WEBKIT_LIBRARIES_LINK_DIR}/AVFoundationCF${DEBUG_SUFFIX}.lib"
    "${WEBKIT_LIBRARIES_LINK_DIR}/QuartzCore${DEBUG_SUFFIX}.lib"
    "${WEBKIT_LIBRARIES_LINK_DIR}/libdispatch${DEBUG_SUFFIX}.lib"
)

WEBKIT_CHECK_HAVE_INCLUDE(HAVE_AVCF AVFoundationCF/AVCFBase.h)

if (HAVE_AVCF)
     SET_AND_EXPOSE_TO_BUILD(USE_AVFOUNDATION ON)
endif ()

WEBKIT_CHECK_HAVE_SYMBOL(HAVE_AVCF_LEGIBLE_OUTPUT AVCFPlayerItemLegibleOutputSetCallbacks "TargetConditionals.h;dispatch/dispatch.h;AVFoundationCF/AVFoundationCF.h;AVFoundationCF/AVCFPlayerItemLegibleOutput.h")
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_AVFOUNDATION_LOADER_DELEGATE AVCFAssetResourceLoaderSetCallbacks "TargetConditionals.h;dispatch/dispatch.h;AVFoundationCF/AVFoundationCF.h")
WEBKIT_CHECK_HAVE_SYMBOL(HAVE_AVCFURL_PLAYABLE_MIMETYPE AVCFURLAssetIsPlayableExtendedMIMEType "TargetConditionals.h;dispatch/dispatch.h;AVFoundationCF/AVFoundationCF.h")

# CMake cannot identify an enum through a symbol check so a source file is required
WEBKIT_CHECK_SOURCE_COMPILES(HAVE_AVCFPLAYERITEM_CALLBACK_VERSION_2 "
#include <AVFoundationCF/AVFoundationCF.h>
#include <AVFoundationCF/AVCFPlayerItemLegibleOutput.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

int main() {
    CFArrayRef types = CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr);
    AVCFPlayerItemLegibleOutputRef legibleOutput = AVCFPlayerItemLegibleOutputCreateWithMediaSubtypesForNativeRepresentation(kCFAllocatorDefault, types);
    AVCFPlayerItemLegibleOutputCallbacks callbackInfo;
    callbackInfo.version = kAVCFPlayerItemLegibleOutput_CallbacksVersion_2;
    dispatch_queue_t dispatchQueue = dispatch_queue_create(\"test\", DISPATCH_QUEUE_SERIAL);
    AVCFPlayerItemLegibleOutputSetCallbacks(legibleOutput, &callbackInfo, dispatchQueue);
}")

if (HAVE_AVCF_LEGIBLE_OUTPUT)
    SET_AND_EXPOSE_TO_BUILD(HAVE_AVFOUNDATION_MEDIA_SELECTION_GROUP ON)
    SET_AND_EXPOSE_TO_BUILD(HAVE_AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT ON)
    SET_AND_EXPOSE_TO_BUILD(HAVE_MEDIA_ACCESSIBILITY_FRAMEWORK ON)
endif ()

WEBKIT_CHECK_HAVE_SYMBOL(HAVE_CACFLAYER_SETCONTENTSSCALE CACFLayerSetContentsScale QuartzCore/CoreAnimationCF.h)

# Warnings as errors (ignore narrowing conversions)
add_compile_options(/WX /Wv:18)

# Support AppleWin internal build by using deprecated directory structure
set(DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources")
set(WTF_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WTF")
set(JavaScriptCore_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore")
set(PAL_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/PAL")
set(WebCore_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebCore")
set(WebDriver_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebDriver")
set(WebKitLegacy_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitLegacy")
set(WebKit_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKit")
set(WebInspectorUI_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebInspectorUI")
set(MiniBrowser_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/MiniBrowser")
set(TestRunnerShared_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/TestRunnerShared")
set(DumpRenderTree_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/DumpRenderTree")
set(WebKitTestRunner_DERIVED_SOURCES_DIR "${CMAKE_BINARY_DIR}/DerivedSources/WebKitTestRunner")

set(FORWARDING_HEADERS_DIR "${DERIVED_SOURCES_DIR}/ForwardingHeaders")
set(bmalloc_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(ANGLE_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WTF_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(JavaScriptCore_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(PAL_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WebKitLegacy_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WebKit_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})
set(WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR ${FORWARDING_HEADERS_DIR})

set(WTF_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/wtf/Scripts")
set(JavaScriptCore_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts")

if (INTERNAL_BUILD)
    set(WTF_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/WTF/Scripts")
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
endif ()
