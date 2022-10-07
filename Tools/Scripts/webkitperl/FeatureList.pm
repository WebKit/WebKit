# Copyright (C) 2012 Google Inc. All rights reserved.
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
use autouse 'webkitdirs' => qw(prohibitUnknownPort);

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
    $applePaySessionV3Support,
    $applePaySessionV4Support,
    $applePaySessionV9Support,
    $applePaySupport,
    $applicationManifestSupport,
    $asyncScrollingSupport,
    $attachmentElementSupport,
    $autocapitalizeSupport,
    $avfCaptionsSupport,
    $bubblewrapSandboxSupport,
    $cachePartitioningSupport,
    $channelMessagingSupport,
    $cloopSupport,
    $contentExtensionsSupport,
    $contentFilteringSupport,
    $contextMenusSupport,
    $cssBoxDecorationBreakSupport,
    $cssCompositingSupport,
    $cssConicGradientsSupport,
    $cssDeviceAdaptationSupport,
    $cssImageResolutionSupport,
    $cssPaintingAPISupport,
    $cssScrollSnapSupport,
    $cssSelectorsLevel4Support,
    $cssTrailingWordSupport,
    $cssTypedOMSupport,
    $cursorVisibilitySupport,
    $darkModeCSSSupport,
    $datacueValueSupport,
    $datalistElementSupport,
    $deviceOrientationSupport,
    $dfgJITSupport,
    $downloadAttributeSupport,
    $dragSupportSupport,
    $encryptedMediaSupport,
    $fatalWarnings,
    $filtersLevel2Support,
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
    $layerBasedSVGEngineSupport,
    $layoutFormattingContextSupport,
    $llvmProfileGenerationSupport,
    $legacyCustomProtocolManagerSupport,
    $legacyEncryptedMediaSupport,
    $letterpressSupport,
    $macGestureEventsSupport,
    $mathmlSupport,
    $mediaCaptureSupport,
    $mediaControlsScriptSupport,
    $mediaSourceSupport,
    $mediaStatisticsSupport,
    $mediaStreamSupport,
    $memorySamplerSupport,
    $meterElementSupport,
    $mhtmlSupport,
    $mouseCursorScaleSupport,
    $navigatorStandaloneSupport,
    $netscapePluginAPISupport,
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
    $publicSuffixListSupport,
    $quotaSupport,
    $remoteInspectorSupport,
    $resolutionMediaQuerySupport,
    $resourceUsageSupport,
    $rubberBandingSupport,
    $samplingProfilerSupport,
    $sandboxExtensionsSupport,
    $serverPreconnectSupport,
    $serviceControlsSupport,
    $serviceWorkerSupport,
    $shareableResourceSupport,
    $smoothScrollingSupport,
    $speechSynthesisSupport,
    $spellcheckSupport,
    $streamsAPISupport,
    $svgFontsSupport,
    $isoMallocSupport,
    $systemMallocSupport,
    $telephoneNumberDetectionSupport,
    $textAutosizingSupport,
    $threeDTransformsSupport,
    $touchEventsSupport,
    $touchSliderSupport,
    $trackingPrevention,
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
    $webAssemblyB3JITSupport,
    $webAudioSupport,
    $webAuthNSupport,
    $webCryptoSupport,
    $webRTCSupport,
    $webdriverKeyboardInteractionsSupport,
    $webdriverMouseInteractionsSupport,
    $webdriverSupport,
    $webdriverTouchInteractionsSupport,
    $webdriverWheelInteractionsSupport,
    $webgl2Support,
    $webglSupport,
    $webXRSupport,
    $wirelessPlaybackTargetSupport,
    $xsltSupport,
);

my @features = (
    { option => "fatal-warnings", desc => "Toggle warnings as errors (CMake only)",
      define => "DEVELOPER_MODE_FATAL_WARNINGS", value => \$fatalWarnings },

    { option => "3d-rendering", desc => "Toggle 3D rendering support",
      define => "ENABLE_3D_TRANSFORMS", value => \$threeDTransformsSupport },

    { option => "accessibility-isolated-tree", desc => "Toggle accessibility isolated tree support",
      define => "ENABLE_ACCESSIBILITY_ISOLATED_TREE", value => \$accessibilityIsolatedTreeSupport },

    { option => "apple-pay", desc => "Toggle Apply Pay support",
      define => "ENABLE_APPLE_PAY", value => \$applePaySupport },

    { option => "apple-pay-session-v9", desc => "Toggle Apple Pay Session V9 support",
      define => "ENABLE_APPLE_PAY_SESSION_V9", value => \$applePaySessionV9Support },

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

    { option => "channel-messaging", desc => "Toggle Channel Messaging support",
      define => "ENABLE_CHANNEL_MESSAGING", value => \$channelMessagingSupport },

    { option => "content-extensions", desc => "Toggle Content Extensions support",
      define => "ENABLE_CONTENT_EXTENSIONS", value => \$contentExtensionsSupport },

    { option => "content-filtering", desc => "Toggle content filtering support",
      define => "ENABLE_CONTENT_FILTERING", value => \$contentFilteringSupport },

    { option => "context-menus", desc => "Toggle Context Menu support",
      define => "ENABLE_CONTEXT_MENUS", value => \$contextMenusSupport },

    { option => "css-box-decoration-break", desc => "Toggle CSS box-decoration-break support",
      define => "ENABLE_CSS_BOX_DECORATION_BREAK", value => \$cssBoxDecorationBreakSupport },

    { option => "css-compositing", desc => "Toggle CSS Compositing support",
      define => "ENABLE_CSS_COMPOSITING", value => \$cssCompositingSupport },

    { option => "css-conic-gradients", desc => "Toggle CSS Conic Gradient support",
      define => "ENABLE_CSS_CONIC_GRADIENTS", value => \$cssConicGradientsSupport },

    { option => "css-device-adaptation", desc => "Toggle CSS Device Adaptation support",
      define => "ENABLE_CSS_DEVICE_ADAPTATION", value => \$cssDeviceAdaptationSupport },

    { option => "css-image-resolution", desc => "Toggle CSS image-resolution support",
      define => "ENABLE_CSS_IMAGE_RESOLUTION", value => \$cssImageResolutionSupport },

    { option => "css-painting-api", desc => "Toggle CSS Painting API support",
      define => "ENABLE_CSS_PAINTING_API", value => \$cssPaintingAPISupport },

    { option => "css-selectors-level4", desc => "Toggle CSS Selectors Level 4 support",
      define => "ENABLE_CSS_SELECTORS_LEVEL4", value => \$cssSelectorsLevel4Support },

    { option => "css-typed-om", desc => "Toggle CSS Typed OM support",
      define => "ENABLE_CSS_TYPED_OM", value => \$cssTypedOMSupport },

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

    { option => "download-attribute", desc => "Toggle Download Attribute support",
      define => "ENABLE_DOWNLOAD_ATTRIBUTE", value => \$downloadAttributeSupport },

    { option => "drag-support", desc => "Toggle support of drag actions (including selection of text with mouse)",
      define => "ENABLE_DRAG_SUPPORT", value => \$dragSupportSupport },

    { option => "encrypted-media", desc => "Toggle EME V3 support",
      define => "ENABLE_ENCRYPTED_MEDIA", value => \$encryptedMediaSupport },

    { option => "filters-level-2", desc => "Toggle Filters Module Level 2",
      define => "ENABLE_FILTERS_LEVEL_2", value => \$filtersLevel2Support },

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

    { option => "layer-based-svg-engine", desc => "Toggle Layer Based SVG Engine support",
      define => "ENABLE_LAYER_BASED_SVG_ENGINE", value => \$layerBasedSVGEngineSupport },

    { option => "layout-formatting-context", desc => "Toggle Layout Formatting Context support",
      define => "ENABLE_LAYOUT_FORMATTING_CONTEXT", value => \$layoutFormattingContextSupport },

    { option => "llvm-profile-generation", desc => "Include LLVM's instrumentation to generate profiles for PGO",
      define => "ENABLE_LLVM_PROFILE_GENERATION", value => \$llvmProfileGenerationSupport },

    { option => "legacy-custom-protocol-manager", desc => "Toggle legacy protocol manager support",
      define => "ENABLE_LEGACY_CUSTOM_PROTOCOL_MANAGER", value => \$legacyCustomProtocolManagerSupport },

    { option => "legacy-encrypted-media", desc => "Toggle Legacy EME V2 support",
      define => "ENABLE_LEGACY_ENCRYPTED_MEDIA", value => \$legacyEncryptedMediaSupport },

    { option => "letterpress", desc => "Toggle letterpress support",
      define => "ENABLE_LETTERPRESS", value => \$letterpressSupport },

    { option => "mac-gesture-events", desc => "Toggle Mac gesture events support",
      define => "ENABLE_MAC_GESTURE_EVENTS", value => \$macGestureEventsSupport },

    { option => "mathml", desc => "Toggle MathML support",
      define => "ENABLE_MATHML", value => \$mathmlSupport },

    { option => "media-capture", desc => "Toggle Media Capture support",
      define => "ENABLE_MEDIA_CAPTURE", value => \$mediaCaptureSupport },

    { option => "media-controls-script", desc => "Toggle definition of media controls in Javascript",
      define => "ENABLE_MEDIA_CONTROLS_SCRIPT", value => \$mediaControlsScriptSupport },

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

    { option => "netscape-plugin-api", desc => "Toggle Netscape Plugin API support",
      define => "ENABLE_NETSCAPE_PLUGIN_API", value => \$netscapePluginAPISupport },

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

    { option => "public-suffix-list", desc => "Toggle public suffix list support",
      define => "ENABLE_PUBLIC_SUFFIX_LIST", value => \$publicSuffixListSupport },

    { option => "remote-inspector", desc => "Toggle remote inspector support",
      define => "ENABLE_REMOTE_INSPECTOR", value => \$remoteInspectorSupport },

    { option => "resolution-media-query", desc => "Toggle resolution media query support",
      define => "ENABLE_RESOLUTION_MEDIA_QUERY", value => \$resolutionMediaQuerySupport },

    { option => "resource-usage", desc => "Toggle resource usage support",
      define => "ENABLE_RESOURCE_USAGE", value => \$resourceUsageSupport },

    { option => "rubber-banding", desc => "Toggle rubber banding support",
      define => "ENABLE_RUBBER_BANDING", value => \$rubberBandingSupport },

    { option => "sampling-profiler", desc => "Toggle sampling profiler support",
      define => "ENABLE_SAMPLING_PROFILER", value => \$samplingProfilerSupport },

    { option => "sandbox-extensions", desc => "Toggle sandbox extensions support",
      define => "ENABLE_SANDBOX_EXTENSIONS", value => \$sandboxExtensionsSupport },

    { option => "server-preconnect", desc => "Toggle server preconnect support",
      define => "ENABLE_SERVER_PRECONNECT", value => \$serverPreconnectSupport },

    { option => "service-controls", desc => "Toggle service controls support",
      define => "ENABLE_SERVICE_CONTROLS", value => \$serviceControlsSupport },

    { option => "service-worker", desc => "Toggle Service Worker support",
      define => "ENABLE_SERVICE_WORKER", value => \$serviceWorkerSupport },

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

    { option => "touch-slider", desc => "Toggle Touch Slider support",
      define => "ENABLE_TOUCH_SLIDER", value => \$touchSliderSupport },

    { option => "tracking-prevention", desc => "Toggle tracking prevention support",
      define => "ENABLE_TRACKING_PREVENTION", value => \$trackingPrevention },

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

    { option => "webassembly-b3jit", desc => "Toggle WebAssembly B3 JIT support",
      define => "ENABLE_WEBASSEMBLY_B3JIT", value => \$webAssemblyB3JITSupport },

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

    { option => "webgl2", desc => "Toggle WebGL2 support",
      define => "ENABLE_WEBGL2", value => \$webgl2Support },

    { option => "webxr", desc => "Toggle WebXR support",
      define => "ENABLE_WEBXR", value => \$webXRSupport },

    { option => "web-api-statistics", desc => "Toggle Web API statistics support",
      define => "ENABLE_WEB_API_STATISTICS", value => \$webAPIStatisticsSupport },

    { option => "web-audio", desc => "Toggle Web Audio support",
      define => "ENABLE_WEB_AUDIO", value => \$webAudioSupport },

    { option => "web-authn", desc => "Toggle Web AuthN support",
      define => "ENABLE_WEB_AUTHN", value => \$webAuthNSupport },

    { option => "web-crypto", desc => "Toggle WebCrypto Subtle-Crypto support",
      define => "ENABLE_WEB_CRYPTO", value => \$webCryptoSupport },

    { option => "web-rtc", desc => "Toggle WebRTC support",
      define => "ENABLE_WEB_RTC", value => \$webRTCSupport },

    { option => "wireless-playback-target", desc => "Toggle wireless playback target support",
      define => "ENABLE_WIRELESS_PLAYBACK_TARGET", value => \$wirelessPlaybackTargetSupport },

    { option => "xslt", desc => "Toggle XSLT support",
      define => "ENABLE_XSLT", value => \$xsltSupport },

    { option => "gstreamer-gl", desc => "Toggle GStreamer GL support",
      define => "USE_GSTREAMER_GL", value => \$gstreamerGLSupport },

    { option => "iso-malloc", desc => "Toggle IsoMalloc support",
      define => "USE_ISO_MALLOC", value => \$isoMallocSupport },

    { option => "system-malloc", desc => "Toggle system allocator instead of bmalloc",
      define => "USE_SYSTEM_MALLOC", value => \$systemMallocSupport },
);

sub getFeatureOptionList()
{
    webkitdirs::prohibitUnknownPort();
    return @features;
}

1;
