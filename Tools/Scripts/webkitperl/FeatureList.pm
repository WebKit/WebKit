# Copyright (C) 2012 Google Inc. All rights reserved.
# Copyright (C) 2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# A module to contain all the enable/disable feature option code.
#
# For CMake ports, this module only affects development builds. The
# settings in this file have ZERO EFFECT for end users. Use
# WebKitFeatures.cmake to change settings for users. Guidelines:
#
# * A feature enabled here but not WebKitFeatures.cmake is EXPERIMENTAL.
# * A feature enabled in WebKitFeatures.cmake but not here is a BUG.

package webkitperl::FeatureList;

use strict;
use warnings;

use FindBin;
use lib $FindBin::Bin;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(&getFeatureOptionList);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

my (
    $accessibilityIsolatedTreeSupport,
    $applePaySupport,
    $applicationManifestSupport,
    $asyncScrollingSupport,
    $attachmentElementSupport,
    $autocapitalizeSupport,
    $avfCaptionsSupport,
    $avifSupport,
    $bubblewrapSandboxSupport,
    $cachePartitioningSupport,
    $cloopSupport,
    $contentExtensionsSupport,
    $contentFilteringSupport,
    $contextMenusSupport,
    $cssDeviceAdaptationSupport,
    $cssImageResolutionSupport,
    $cssScrollSnapSupport,
    $cssTrailingWordSupport,
    $cursorVisibilitySupport,
    $darkModeCSSSupport,
    $datacueValueSupport,
    $datalistElementSupport,
    $deviceOrientationSupport,
    $dfgJITSupport,
    $dragSupportSupport,
    $encryptedMediaSupport,
    $fatalWarnings,
    $ftlJITSupport,
    $ftpDirSupport,
    $fullscreenAPISupport,
    $gamepadSupport,
    $geolocationSupport,
    $gpuProcessSupport,
    $gstreamerGLSupport,
    $inputTypeColorSupport,
    $inputTypeDateSupport,
    $inputTypeDatetimelocalSupport,
    $inputTypeMonthSupport,
    $inputTypeTimeSupport,
    $inputTypeWeekSupport,
    $inspectorAlternateDispatchersSupport,
    $inspectorTelemetrySupport,
    $iosGestureEventsSupport,
    $iosTouchEventsSupport,
    $jitSupport,
    $jpegXLSupport,
    $llvmProfileGenerationSupport,
    $legacyCustomProtocolManagerSupport,
    $legacyEncryptedMediaSupport,
    $macGestureEventsSupport,
    $mathmlSupport,
    $mediaCaptureSupport,
    $mediaSourceSupport,
    $mediaStatisticsSupport,
    $mediaStreamSupport,
    $memorySamplerSupport,
    $meterElementSupport,
    $mhtmlSupport,
    $lcmsSupport,
    $mouseCursorScaleSupport,
    $navigatorStandaloneSupport,
    $networkCacheSpeculativeRevalidationSupport,
    $networkCacheStaleWhileRevalidateSupport,
    $notificationsSupport,
    $offscreenCanvasSupport,
    $offscreenCanvasInWorkersSupport,
    $thunderSupport,
    $orientationEventsSupport,
    $overflowScrollingTouchSupport,
    $paymentRequestSupport,
    $pdfJS,
    $pdfkitPluginSupport,
    $pictureInPictureAPISupport,
    $pointerLockSupport,
    $quotaSupport,
    $remoteInspectorSupport,
    $resourceUsageSupport,
    $samplingProfilerSupport,
    $sandboxExtensionsSupport,
    $serverPreconnectSupport,
    $serviceControlsSupport,
    $shareableResourceSupport,
    $skiaSupport,
    $smoothScrollingSupport,
    $speechSynthesisSupport,
    $spellcheckSupport,
    $svgFontsSupport,
    $isoMallocSupport,
    $systemMallocSupport,
    $telephoneNumberDetectionSupport,
    $textAutosizingSupport,
    $touchEventsSupport,
    $unifiedBuildsSupport,
    $userMessageHandlersSupport,
    $userselectAllSupport,
    $variationFontsSupport,
    $videoPresentationModeSupport,
    $videoSupport,
    $videoTrackSupport,
    $videoUsesElementFullscreenSupport,
    $webAPIStatisticsSupport,
    $webAssemblySupport,
    $webAudioSupport,
    $webAuthNSupport,
    $webCodecsSupport,
    $wkWebExtensionsSupport,
    $webRTCSupport,
    $webdriverKeyboardInteractionsSupport,
    $webdriverMouseInteractionsSupport,
    $webdriverSupport,
    $webdriverTouchInteractionsSupport,
    $webdriverWheelInteractionsSupport,
    $webglSupport,
    $webGpuSwift,
    $webXRSupport,
    $wirelessPlaybackTargetSupport,
    $woff2Support,
    $xsltSupport,
);

my @features = (
    { option => "fatal-warnings", desc => "Toggle warnings as errors (CMake only)",
      define => "DEVELOPER_MODE_FATAL_WARNINGS", value => \$fatalWarnings },

    { option => "accessibility-isolated-tree", desc => "Toggle accessibility isolated tree support",
      define => "ENABLE_ACCESSIBILITY_ISOLATED_TREE", value => \$accessibilityIsolatedTreeSupport },

    { option => "apple-pay", desc => "Toggle Apply Pay support",
      define => "ENABLE_APPLE_PAY", value => \$applePaySupport },

    { option => "application-manifest", desc => "Toggle Application Manifest support",
      define => "ENABLE_APPLICATION_MANIFEST", value => \$applicationManifestSupport },

    { option => "async-scrolling", desc => "Enable asynchronous scrolling",
      define => "ENABLE_ASYNC_SCROLLING", value => \$asyncScrollingSupport },

    { option => "attachment-element", desc => "Toggle Attachment Element support",
      define => "ENABLE_ATTACHMENT_ELEMENT", value => \$attachmentElementSupport },

    { option => "autocapitalize", desc => "Toggle autocapitalize support",
      define => "ENABLE_AUTOCAPITALIZE", value => \$autocapitalizeSupport },

    { option => "avf-captions", desc => "Toggle AVFoundation caption support",
      define => "ENABLE_AVF_CAPTIONS", value => \$avfCaptionsSupport },

    { option => "bubblewrap-sandbox", desc => "Toggle Bubblewrap sandboxing support",
      define => "ENABLE_BUBBLEWRAP_SANDBOX", value => \$bubblewrapSandboxSupport },

    { option => "cache-partitioning", desc => "Toggle cache partitioning support",
      define => "ENABLE_CACHE_PARTITIONING", value => \$cachePartitioningSupport },

    { option => "content-extensions", desc => "Toggle Content Extensions support",
      define => "ENABLE_CONTENT_EXTENSIONS", value => \$contentExtensionsSupport },

    { option => "content-filtering", desc => "Toggle content filtering support",
      define => "ENABLE_CONTENT_FILTERING", value => \$contentFilteringSupport },

    { option => "context-menus", desc => "Toggle Context Menu support",
      define => "ENABLE_CONTEXT_MENUS", value => \$contextMenusSupport },

    { option => "cursor-visibility", desc => "Toggle cursor visibility support",
      define => "ENABLE_CURSOR_VISIBILITY", value => \$cursorVisibilitySupport },

    { option => "cloop", desc => "Enable CLoop interpreter",
      define => "ENABLE_C_LOOP", value => \$cloopSupport },

    { option => "dark-mode-css", desc => "Toggle Dark Mode CSS support",
      define => "ENABLE_DARK_MODE_CSS", value => \$darkModeCSSSupport },

    { option => "datacue-value", desc => "Toggle datacue value support",
      define => "ENABLE_DATACUE_VALUE", value => \$datacueValueSupport },

    { option => "datalist-element", desc => "Toggle Datalist Element support",
      define => "ENABLE_DATALIST_ELEMENT", value => \$datalistElementSupport },

    { option => "device-orientation", desc => "Toggle Device Orientation support",
      define => "ENABLE_DEVICE_ORIENTATION", value => \$deviceOrientationSupport },

    { option => "dfg-jit", desc => "Toggle data flow graph JIT tier",
      define => "ENABLE_DFG_JIT", value => \$dfgJITSupport },

    { option => "drag-support", desc => "Toggle support of drag actions (including selection of text with mouse)",
      define => "ENABLE_DRAG_SUPPORT", value => \$dragSupportSupport },

    { option => "encrypted-media", desc => "Toggle EME V3 support",
      define => "ENABLE_ENCRYPTED_MEDIA", value => \$encryptedMediaSupport },

    { option => "ftl-jit", desc => "Toggle FTL JIT support",
      define => "ENABLE_FTL_JIT", value => \$ftlJITSupport },

    { option => "ftpdir", desc => "Toggle FTP Directory support",
      define => "ENABLE_FTPDIR", value => \$ftpDirSupport },

    { option => "fullscreen-api", desc => "Toggle Fullscreen API support",
      define => "ENABLE_FULLSCREEN_API", value => \$fullscreenAPISupport },

    { option => "gamepad", desc => "Toggle Gamepad support",
      define => "ENABLE_GAMEPAD", value => \$gamepadSupport },

    { option => "geolocation", desc => "Toggle Geolocation support",
      define => "ENABLE_GEOLOCATION", value => \$geolocationSupport },

    { option => "gpu-process", desc => "Toggle GPU Process support",
      define => "ENABLE_GPU_PROCESS", value => \$gpuProcessSupport },

    { option => "input-type-color", desc => "Toggle Input Type Color support",
      define => "ENABLE_INPUT_TYPE_COLOR", value => \$inputTypeColorSupport },

    { option => "input-type-date", desc => "Toggle Input Type Date support",
      define => "ENABLE_INPUT_TYPE_DATE", value => \$inputTypeDateSupport },

    { option => "input-type-datetimelocal", desc => "Toggle Input Type Datetimelocal support",
      define => "ENABLE_INPUT_TYPE_DATETIMELOCAL", value => \$inputTypeDatetimelocalSupport },

    { option => "input-type-month", desc => "Toggle Input Type Month support",
      define => "ENABLE_INPUT_TYPE_MONTH", value => \$inputTypeMonthSupport },

    { option => "input-type-time", desc => "Toggle Input Type Time support",
      define => "ENABLE_INPUT_TYPE_TIME", value => \$inputTypeTimeSupport },

    { option => "input-type-week", desc => "Toggle Input Type Week support",
      define => "ENABLE_INPUT_TYPE_WEEK", value => \$inputTypeWeekSupport },

    { option => "inspector-alternate-dispatchers", desc => "Toggle inspector alternate dispatchers support",
      define => "ENABLE_INSPECTOR_ALTERNATE_DISPATCHERS", value => \$inspectorAlternateDispatchersSupport },

    { option => "inspector-telemetry", desc => "Toggle inspector telemetry support",
      define => "ENABLE_INSPECTOR_TELEMETRY", value => \$inspectorTelemetrySupport },

    { option => "ios-gesture-events", desc => "Toggle iOS gesture events support",
      define => "ENABLE_IOS_GESTURE_EVENTS", value => \$iosGestureEventsSupport },

    { option => "ios-touch-events", desc => "Toggle iOS touch events support",
      define => "ENABLE_IOS_TOUCH_EVENTS", value => \$iosTouchEventsSupport },

    { option => "jit", desc => "Toggle JustInTime JavaScript support",
      define => "ENABLE_JIT", value => \$jitSupport },

    { option => "llvm-profile-generation", desc => "Include LLVM's instrumentation to generate profiles for PGO",
      define => "ENABLE_LLVM_PROFILE_GENERATION", value => \$llvmProfileGenerationSupport },

    { option => "legacy-custom-protocol-manager", desc => "Toggle legacy protocol manager support",
      define => "ENABLE_LEGACY_CUSTOM_PROTOCOL_MANAGER", value => \$legacyCustomProtocolManagerSupport },

    { option => "legacy-encrypted-media", desc => "Toggle Legacy EME V2 support",
      define => "ENABLE_LEGACY_ENCRYPTED_MEDIA", value => \$legacyEncryptedMediaSupport },

    { option => "mac-gesture-events", desc => "Toggle Mac gesture events support",
      define => "ENABLE_MAC_GESTURE_EVENTS", value => \$macGestureEventsSupport },

    { option => "mathml", desc => "Toggle MathML support",
      define => "ENABLE_MATHML", value => \$mathmlSupport },

    { option => "media-capture", desc => "Toggle Media Capture support",
      define => "ENABLE_MEDIA_CAPTURE", value => \$mediaCaptureSupport },

    { option => "media-source", desc => "Toggle Media Source support",
      define => "ENABLE_MEDIA_SOURCE", value => \$mediaSourceSupport },

    { option => "media-statistics", desc => "Toggle Media Statistics support",
      define => "ENABLE_MEDIA_STATISTICS", value => \$mediaStatisticsSupport },

    { option => "media-stream", desc => "Toggle Media Stream support",
      define => "ENABLE_MEDIA_STREAM", value => \$mediaStreamSupport },

    { option => "memory-sampler", desc => "Toggle Memory Sampler support",
      define => "ENABLE_MEMORY_SAMPLER", value => \$memorySamplerSupport },

    { option => "mhtml", desc => "Toggle MHTML support",
      define => "ENABLE_MHTML", value => \$mhtmlSupport },

    { option => "mouse-cursor-scale", desc => "Toggle Scaled mouse cursor support",
      define => "ENABLE_MOUSE_CURSOR_SCALE", value => \$mouseCursorScaleSupport },

    { option => "navigator-standalone", desc => "Toogle standalone navigator support",
      define => "ENABLE_NAVIGATOR_STANDALONE", value => \$navigatorStandaloneSupport },

    { option => "network-cache-speculative-revalidation", desc => "Toogle network cache speculative revalidation support",
      define => "ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION", value => \$networkCacheSpeculativeRevalidationSupport },

    { option => "network-cache-stale-while-revalidate", desc => "Toogle network cache stale-while-revalidate caching strategy",
      define => "ENABLE_NETWORK_CACHE_STALE_WHILE_REVALIDATE", value => \$networkCacheStaleWhileRevalidateSupport },

    { option => "notifications", desc => "Toggle Notifications support",
      define => "ENABLE_NOTIFICATIONS", value => \$notificationsSupport },

    { option => "offscreen-canvas", desc => "Toggle OffscreenCanvas support",
      define => "ENABLE_OFFSCREEN_CANVAS", value => \$offscreenCanvasSupport },

    { option => "offscreen-canvas-in-workers", desc => "Toggle OffscreenCanvas in Workers support",
      define => "ENABLE_OFFSCREEN_CANVAS_IN_WORKERS", value => \$offscreenCanvasInWorkersSupport },

    { option => "thunder", desc => "Toggle Thunder CDM support",
      define => "ENABLE_THUNDER", value => \$thunderSupport },

    { option => "orientation-events", desc => "Toggle Orientation Events support",
      define => "ENABLE_ORIENTATION_EVENTS", value => \$orientationEventsSupport },

    { option => "overflow-scrolling-touch", desc => "Toggle accelerated scrolling support",
      define => "ENABLE_OVERFLOW_SCROLLING_TOUCH", value => \$overflowScrollingTouchSupport },

    { option => "payment-request", desc => "Toggle Payment Request support",
      define => "ENABLE_PAYMENT_REQUEST", value => \$paymentRequestSupport },

    { option => "pdfjs", desc => "Toggle PDF.js integration",
      define => "ENABLE_PDFJS", value => \$pdfJS },

    { option => "pdfkit-plugin", desc => "Toggle PDFKit plugin support",
      define => "ENABLE_PDFKIT_PLUGIN", value => \$pdfkitPluginSupport },

    { option => "picture-in-picture-api", desc => "Toggle Picture-in-Picture API support",
      define => "ENABLE_PICTURE_IN_PICTURE_API", value => \$pictureInPictureAPISupport },

    { option => "pointer-lock", desc => "Toggle pointer lock support",
      define => "ENABLE_POINTER_LOCK", value => \$pointerLockSupport },

    { option => "remote-inspector", desc => "Toggle remote inspector support",
      define => "ENABLE_REMOTE_INSPECTOR", value => \$remoteInspectorSupport },

    { option => "resource-usage", desc => "Toggle resource usage support",
      define => "ENABLE_RESOURCE_USAGE", value => \$resourceUsageSupport },

    { option => "sampling-profiler", desc => "Toggle sampling profiler support",
      define => "ENABLE_SAMPLING_PROFILER", value => \$samplingProfilerSupport },

    { option => "sandbox-extensions", desc => "Toggle sandbox extensions support",
      define => "ENABLE_SANDBOX_EXTENSIONS", value => \$sandboxExtensionsSupport },

    { option => "server-preconnect", desc => "Toggle server preconnect support",
      define => "ENABLE_SERVER_PRECONNECT", value => \$serverPreconnectSupport },

    { option => "service-controls", desc => "Toggle service controls support",
      define => "ENABLE_SERVICE_CONTROLS", value => \$serviceControlsSupport },

    { option => "shareable-resource", desc => "Toggle network shareable resources support",
      define => "ENABLE_SHAREABLE_RESOURCE", value => \$shareableResourceSupport },

    { option => "smooth-scrolling", desc => "Toggle smooth scrolling",
      define => "ENABLE_SMOOTH_SCROLLING", value => \$smoothScrollingSupport },

    { option => "speech-synthesis", desc => "Toggle Speech Synthesis API support",
      define => "ENABLE_SPEECH_SYNTHESIS", value => \$speechSynthesisSupport },

    { option => "spellcheck", desc => "Toggle Spellchecking support (requires Enchant)",
      define => "ENABLE_SPELLCHECK", value => \$spellcheckSupport },

    { option => "telephone-number-detection", desc => "Toggle telephone number detection support",
      define => "ENABLE_TELEPHONE_NUMBER_DETECTION", value => \$telephoneNumberDetectionSupport },

    { option => "text-autosizing", desc => "Toggle automatic text size adjustment support",
      define => "ENABLE_TEXT_AUTOSIZING", value => \$textAutosizingSupport },

    { option => "touch-events", desc => "Toggle Touch Events support",
      define => "ENABLE_TOUCH_EVENTS", value => \$touchEventsSupport },

    { option => "unified-builds", desc => "Toggle unified builds",
      define => "ENABLE_UNIFIED_BUILDS", value => \$unifiedBuildsSupport },

    { option => "user-message-handlers", desc => "Toggle user script message handler support",
      define => "ENABLE_USER_MESSAGE_HANDLERS", value => \$userMessageHandlersSupport },

    { option => "variation-fonts", desc => "Toggle variation fonts support",
      define => "ENABLE_VARIATION_FONTS", value => \$variationFontsSupport },

    { option => "video", desc => "Toggle Video support",
      define => "ENABLE_VIDEO", value => \$videoSupport },

    { option => "video-presentation-mode", desc => "Toggle Video presentation mode support",
      define => "ENABLE_VIDEO_PRESENTATION_MODE", value => \$videoPresentationModeSupport },

    { option => "video-uses-element-fullscreen", desc => "Toggle video element fullscreen support",
      define => "ENABLE_VIDEO_USES_ELEMENT_FULLSCREEN", value => \$videoUsesElementFullscreenSupport },

    { option => "webassembly", desc => "Toggle WebAssembly support",
      define => "ENABLE_WEBASSEMBLY", value => \$webAssemblySupport },

    { option => "webdriver", desc => "Toggle WebDriver service process",
      define => "ENABLE_WEBDRIVER", value => \$webdriverSupport },

    { option => "webdriver-keyboard-interactions", desc => "Toggle WebDriver keyboard interactions",
      define => "ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS", value => \$webdriverKeyboardInteractionsSupport },

    { option => "webdriver-mouse-interactions", desc => "Toggle WebDriver mouse interactions",
      define => "ENABLE_WEBDRIVER_MOUSE_INTERACTIONS", value => \$webdriverMouseInteractionsSupport },

    { option => "webdriver-touch-interactions", desc => "Toggle WebDriver touch interactions",
      define => "ENABLE_WEBDRIVER_TOUCH_INTERACTIONS", value => \$webdriverTouchInteractionsSupport },

    { option => "webdriver-wheel-interactions", desc => "Toggle WebDriver wheel interactions",
      define => "ENABLE_WEBDRIVER_WHEEL_INTERACTIONS", value => \$webdriverWheelInteractionsSupport },

    { option => "webgl", desc => "Toggle WebGL support",
      define => "ENABLE_WEBGL", value => \$webglSupport },

    { option => "webGpuSwift", desc => "Toggle WebGpu Swift Implementation",
      define => "ENABLE_WEBGPU_SWIFT", value => \$webGpuSwift },

    { option => "webxr", desc => "Toggle WebXR support",
      define => "ENABLE_WEBXR", value => \$webXRSupport },

    { option => "web-api-statistics", desc => "Toggle Web API statistics support",
      define => "ENABLE_WEB_API_STATISTICS", value => \$webAPIStatisticsSupport },

    { option => "web-audio", desc => "Toggle Web Audio support",
      define => "ENABLE_WEB_AUDIO", value => \$webAudioSupport },

    { option => "web-authn", desc => "Toggle Web AuthN support",
      define => "ENABLE_WEB_AUTHN", value => \$webAuthNSupport },

    { option => "web-codecs", desc => "Toggle WebCodecs support",
      define => "ENABLE_WEB_CODECS", value => \$webCodecsSupport },

    { option => "web-rtc", desc => "Toggle WebRTC support",
      define => "ENABLE_WEB_RTC", value => \$webRTCSupport },

    { option => "wireless-playback-target", desc => "Toggle wireless playback target support",
      define => "ENABLE_WIRELESS_PLAYBACK_TARGET", value => \$wirelessPlaybackTargetSupport },
    
    { option => "wk-web-extensions", desc => "Toggle WebExtensions support",
      define => "ENABLE_WK_WEB_EXTENSIONS", value => \$wkWebExtensionsSupport },

    { option => "xslt", desc => "Toggle XSLT support",
      define => "ENABLE_XSLT", value => \$xsltSupport },

    { option => "avif", desc => "Toggle support for AVIF images",
      define => "USE_AVIF", value => \$avifSupport },

    { option => "iso-malloc", desc => "Toggle IsoMalloc support",
      define => "USE_ISO_MALLOC", value => \$isoMallocSupport },

    { option => "jpegxl", desc => "Toggle support for JPEG XL images",
      define => "USE_JPEGXL", value => \$jpegXLSupport },

    { option => "lcms", desc => "Toggle support for image color management using libcms2",
      define => "USE_LCMS", value => \$lcmsSupport },

    { option => "skia", desc => "Toggle Skia instead of Cairo for rasterization",
      define => "USE_SKIA", value => \$skiaSupport },

    { option => "system-malloc", desc => "Toggle system allocator instead of bmalloc",
      define => "USE_SYSTEM_MALLOC", value => \$systemMallocSupport },

    { option => "woff2", desc => "Toggle support for WOFF2 Web Fonts through libwoff2",
      define => "USE_WOFF2", value => \$woff2Support },
);

sub getFeatureOptionList()
{
    webkitdirs::prohibitUnknownPort();
    return @features;
}

1;
