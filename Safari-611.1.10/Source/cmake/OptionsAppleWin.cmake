# The static runtime used to be required for AppleWin due to
# WebKitSystemInterface.lib being compiled with a static runtime. That library
# is no longer used, but we keep building with static runtime for backward
# compatibility. But if someone decides that it's OK to require existing
# projects to build with the runtime DLLs, that's now technically possible.
set(MSVC_STATIC_RUNTIME ON)

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

SET_AND_EXPOSE_TO_BUILD(USE_CF ON)
SET_AND_EXPOSE_TO_BUILD(USE_CFURLCONNECTION ON)

set(SQLite3_NAMES SQLite3${DEBUG_SUFFIX})

find_package(ICU 60.2 REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 REQUIRED)
find_package(LibXslt REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)

# Libraries where find_package does not work
set(COREFOUNDATION_LIBRARY CoreFoundation${DEBUG_SUFFIX})

# Uncomment the following line to try the Direct2D backend.
# set(USE_DIRECT2D 1)

if (${USE_DIRECT2D})
    SET_AND_EXPOSE_TO_BUILD(USE_DIRECT2D ON)
else ()
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
endif ()

# Warnings as errors (ignore narrowing conversions)
add_compile_options(/WX /Wv:18)

if (INTERNAL_BUILD)
    set(WTF_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/WTF/Scripts")
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
endif ()
