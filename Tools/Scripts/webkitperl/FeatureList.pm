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

use strict;
use warnings;

use FindBin;
use lib $FindBin::Bin;
use webkitdirs;

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
    $threeDTransformsSupport,
    $accelerated2DCanvasSupport,
    $allInOneBuild,
    $arrowfunctionSyntax,
    $attachmentElementSupport,
    $batteryStatusSupport,
    $canvasPathSupport,
    $canvasProxySupport,
    $channelMessagingSupport,
    $classSyntax,
    $templateLiteralSyntax,
    $cspNextSupport,
    $css3TextSupport,
    $css3TextLineBreakSupport,
    $css4ImagesSupport,
    $cssBoxDecorationBreakSupport,
    $cssDeviceAdaptation,
    $cssGridLayoutSupport,
    $cssImageOrientationSupport,
    $cssImageResolutionSupport,
    $cssImageSetSupport,
    $cssRegionsSupport,
    $cssShapesSupport,
    $cssCompositingSupport,
    $customElementsSupport,
    $customSchemeHandlerSupport,
    $dataTransferItemsSupport,
    $datalistElementSupport,
    $detailsElementSupport,
    $deviceOrientationSupport,
    $directoryUploadSupport,
    $dom4EventsConstructor,
    $downloadAttributeSupport,
    $fetchAPISupport,
    $fontLoadEventsSupport,
    $ftpDirSupport,
    $fullscreenAPISupport,
    $gamepadSupport,
    $generatorsSupport,
    $geolocationSupport,
    $hardwareConcurrencySupport,
    $highDPICanvasSupport,
    $icondatabaseSupport,
    $indexedDatabaseSupport,
    $inputSpeechSupport,
    $inputTypeColorSupport,
    $inputTypeDateSupport,
    $inputTypeDatetimeSupport,
    $inputTypeDatetimelocalSupport,
    $inputTypeMonthSupport,
    $inputTypeTimeSupport,
    $inputTypeWeekSupport,
    $intlSupport,
    $jitSupport,
    $legacyNotificationsSupport,
    $legacyVendorPrefixSupport,
    $legacyWebAudioSupport,
    $linkPrefetchSupport,
    $linkPrerenderSupport,
    $mathmlSupport,
    $mediaCaptureSupport,
    $mediaSourceSupport,
    $mediaStatisticsSupport,
    $mediaStreamSupport,
    $meterElementSupport,
    $mhtmlSupport,
    $modulesSupport,
    $mouseCursorScaleSupport,
    $netscapePluginAPISupport,
    $nosniffSupport,
    $notificationsSupport,
    $orientationEventsSupport,
    $pageVisibilityAPISupport,
    $performanceTimelineSupport,
    $promiseSupport,
    $proximityEventsSupport,
    $quotaSupport,
    $resolutionMediaQuerySupport,
    $registerProtocolHandlerSupport,
    $requestAnimationFrameSupport,
    $resourceTimingSupport,
    $scriptedSpeechSupport,
    $seccompFiltersSupport,
    $shadowDOMSupport,
    $streamsAPISupport,
    $styleScopedSupport,
    $subtleCrypto,
    $svgDOMObjCBindingsSupport,
    $svgFontsSupport,
    $systemMallocSupport,
    $templateElementSupport,
    $textAutosizingSupport,
    $threadedCompositorSupport,
    $threadedHTMLParserSupport,
    $touchEventsSupport,
    $touchSliderSupport,
    $touchIconLoadingSupport,
    $userTimingSupport,
    $vibrationSupport,
    $videoSupport,
    $videoTrackSupport,
    $webglSupport,
    $webAssemblySupport,
    $webAnimationsSupport,
    $webAudioSupport,
    $webReplaySupport,
    $webSocketsSupport,
    $webTimingSupport,
    $xsltSupport,
    $ftlJITSupport,
);

my @features = (
    { option => "3d-rendering", desc => "Toggle 3D Rendering support",
      define => "ENABLE_3D_TRANSFORMS", default => (isAppleMacWebKit() || isIOSWebKit() || isGtk() || isEfl()), value => \$threeDTransformsSupport },

    { option => "accelerated-2d-canvas", desc => "Toggle Accelerated 2D Canvas support",
      define => "ENABLE_ACCELERATED_2D_CANVAS", default => isGtk(), value => \$accelerated2DCanvasSupport },

    { option => "allinone-build", desc => "Toggle all-in-one build",
      define => "ENABLE_ALLINONE_BUILD", default => isWindows(), value => \$allInOneBuild },

    { option => "arrowfunction-syntax", desc => "Toggle ES6 arrow function syntax support",
      define => "ENABLE_ES6_ARROWFUNCTION_SYNTAX", default => 1, value => \$arrowfunctionSyntax },

    { option => "attachment-element", desc => "Toggle Attachment Element support",
      define => "ENABLE_ATTACHMENT_ELEMENT", default => 0, value => \$attachmentElementSupport },

    { option => "battery-status", desc => "Toggle Battery Status support",
      define => "ENABLE_BATTERY_STATUS", default => isEfl(), value => \$batteryStatusSupport },

    { option => "canvas-path", desc => "Toggle Canvas Path support",
      define => "ENABLE_CANVAS_PATH", default => 1, value => \$canvasPathSupport },

    { option => "canvas-proxy", desc => "Toggle CanvasProxy support",
      define => "ENABLE_CANVAS_PROXY", default => 0, value => \$canvasProxySupport },

    { option => "channel-messaging", desc => "Toggle Channel Messaging support",
      define => "ENABLE_CHANNEL_MESSAGING", default => 1, value => \$channelMessagingSupport },

    { option => "class-syntax", desc => "Toggle ES6 class syntax support",
      define => "ENABLE_ES6_CLASS_SYNTAX", default => 1, value => \$classSyntax },

    { option => "generators", desc => "Toggle ES6 generators support",
      define => "ENABLE_ES6_GENERATORS", default => 1, value => \$generatorsSupport },

    { option => "modules", desc => "Toggle ES6 modules support",
      define => "ENABLE_ES6_MODULES", default => 0, value => \$modulesSupport },

    { option => "template-literal-syntax", desc => "Toggle ES6 template literal syntax support",
      define => "ENABLE_ES6_TEMPLATE_LITERAL_SYNTAX", default => 1, value => \$templateLiteralSyntax },

    { option => "csp-next", desc => "Toggle Content Security Policy 1.1 support",
      define => "ENABLE_CSP_NEXT", default => isGtk(), value => \$cspNextSupport },

    { option => "css-device-adaptation", desc => "Toggle CSS Device Adaptation support",
      define => "ENABLE_CSS_DEVICE_ADAPTATION", default => isEfl(), value => \$cssDeviceAdaptation },

    { option => "css-shapes", desc => "Toggle CSS Shapes support",
      define => "ENABLE_CSS_SHAPES", default => 1, value => \$cssShapesSupport },

    { option => "css-grid-layout", desc => "Toggle CSS Grid Layout support",
      define => "ENABLE_CSS_GRID_LAYOUT", default => 1, value => \$cssGridLayoutSupport },

    { option => "css3-text", desc => "Toggle CSS3 Text support",
      define => "ENABLE_CSS3_TEXT", default => (isEfl() || isGtk()), value => \$css3TextSupport },

    { option => "css3-text-line-break", desc => "Toggle CSS3 Text Line Break support",
      define => "ENABLE_CSS3_TEXT_LINE_BREAK", default => 0, value => \$css3TextLineBreakSupport },

    { option => "css-box-decoration-break", desc => "Toggle CSS box-decoration-break support",
      define => "ENABLE_CSS_BOX_DECORATION_BREAK", default => 1, value => \$cssBoxDecorationBreakSupport },

    { option => "css-image-orientation", desc => "Toggle CSS image-orientation support",
      define => "ENABLE_CSS_IMAGE_ORIENTATION", default => (isEfl() || isGtk()), value => \$cssImageOrientationSupport },

    { option => "css-image-resolution", desc => "Toggle CSS image-resolution support",
      define => "ENABLE_CSS_IMAGE_RESOLUTION", default => isGtk(), value => \$cssImageResolutionSupport },

    { option => "css-image-set", desc => "Toggle CSS image-set support",
      define => "ENABLE_CSS_IMAGE_SET", default => (isEfl() || isGtk()), value => \$cssImageSetSupport },

    { option => "css-regions", desc => "Toggle CSS Regions support",
      define => "ENABLE_CSS_REGIONS", default => 1, value => \$cssRegionsSupport },

    { option => "css-compositing", desc => "Toggle CSS Compositing support",
      define => "ENABLE_CSS_COMPOSITING", default => isAppleWebKit(), value => \$cssCompositingSupport },

    { option => "custom-elements", desc => "Toggle custom elements support",
      define => "ENABLE_CUSTOM_ELEMENTS", default => (isAppleMacWebKit() || isIOSWebKit()), value => \$customElementsSupport },

    { option => "custom-scheme-handler", desc => "Toggle Custom Scheme Handler support",
      define => "ENABLE_CUSTOM_SCHEME_HANDLER", default => isEfl(), value => \$customSchemeHandlerSupport },

    { option => "datalist-element", desc => "Toggle Datalist Element support",
      define => "ENABLE_DATALIST_ELEMENT", default => isEfl(), value => \$datalistElementSupport },

    { option => "data-transfer-items", desc => "Toggle Data Transfer Items support",
      define => "ENABLE_DATA_TRANSFER_ITEMS", default => 0, value => \$dataTransferItemsSupport },

    { option => "details-element", desc => "Toggle Details Element support",
      define => "ENABLE_DETAILS_ELEMENT", default => 1, value => \$detailsElementSupport },

    { option => "device-orientation", desc => "Toggle Device Orientation support",
      define => "ENABLE_DEVICE_ORIENTATION", default => isIOSWebKit(), value => \$deviceOrientationSupport },

    { option => "dom4-events-constructor", desc => "Expose DOM4 Events constructors",
      define => "ENABLE_DOM4_EVENTS_CONSTRUCTOR", default => (isAppleWebKit() || isGtk() || isEfl()), value => \$dom4EventsConstructor },

    { option => "download-attribute", desc => "Toggle Download Attribute support",
      define => "ENABLE_DOWNLOAD_ATTRIBUTE", default => isEfl(), value => \$downloadAttributeSupport },

    { option => "fetch-api", desc => "Toggle Fetch API support",
      define => "ENABLE_FETCH_API", default => 1, value => \$fetchAPISupport },

    { option => "font-load-events", desc => "Toggle Font Load Events support",
      define => "ENABLE_FONT_LOAD_EVENTS", default => 0, value => \$fontLoadEventsSupport },

    { option => "ftpdir", desc => "Toggle FTP Directory support",
      define => "ENABLE_FTPDIR", default => 1, value => \$ftpDirSupport },

    { option => "fullscreen-api", desc => "Toggle Fullscreen API support",
      define => "ENABLE_FULLSCREEN_API", default => (isAppleMacWebKit() || isEfl() || isGtk()), value => \$fullscreenAPISupport },

    { option => "gamepad", desc => "Toggle Gamepad support",
      define => "ENABLE_GAMEPAD", default => 0, value => \$gamepadSupport },

    { option => "geolocation", desc => "Toggle Geolocation support",
      define => "ENABLE_GEOLOCATION", default => (isAppleWebKit() || isIOSWebKit() || isGtk() || isEfl()), value => \$geolocationSupport },

    { option => "high-dpi-canvas", desc => "Toggle High DPI Canvas support",
      define => "ENABLE_HIGH_DPI_CANVAS", default => (isAppleWebKit()), value => \$highDPICanvasSupport },

    { option => "icon-database", desc => "Toggle Icondatabase support",
      define => "ENABLE_ICONDATABASE", default => !isIOSWebKit(), value => \$icondatabaseSupport },

    { option => "indexed-database", desc => "Toggle Indexed Database support",
      define => "ENABLE_INDEXED_DATABASE", default => (isEfl() || isGtk()), value => \$indexedDatabaseSupport },

    { option => "input-speech", desc => "Toggle Input Speech support",
      define => "ENABLE_INPUT_SPEECH", default => 0, value => \$inputSpeechSupport },

    { option => "input-type-color", desc => "Toggle Input Type Color support",
      define => "ENABLE_INPUT_TYPE_COLOR", default => (isEfl() || isGtk()), value => \$inputTypeColorSupport },

    { option => "input-type-date", desc => "Toggle Input Type Date support",
      define => "ENABLE_INPUT_TYPE_DATE", default => 0, value => \$inputTypeDateSupport },

    { option => "input-type-datetime", desc => "Toggle broken Input Type Datetime support",
      define => "ENABLE_INPUT_TYPE_DATETIME_INCOMPLETE", default => 0, value => \$inputTypeDatetimeSupport },

    { option => "input-type-datetimelocal", desc => "Toggle Input Type Datetimelocal support",
      define => "ENABLE_INPUT_TYPE_DATETIMELOCAL", default => 0, value => \$inputTypeDatetimelocalSupport },

    { option => "input-type-month", desc => "Toggle Input Type Month support",
      define => "ENABLE_INPUT_TYPE_MONTH", default => 0, value => \$inputTypeMonthSupport },

    { option => "input-type-time", desc => "Toggle Input Type Time support",
      define => "ENABLE_INPUT_TYPE_TIME", default => 0, value => \$inputTypeTimeSupport },

    { option => "input-type-week", desc => "Toggle Input Type Week support",
      define => "ENABLE_INPUT_TYPE_WEEK", default => 0, value => \$inputTypeWeekSupport },

    { option => "intl", desc => "Toggle Intl support",
      define => "ENABLE_INTL", default => 1, value => \$intlSupport },

    { option => "legacy-notifications", desc => "Toggle Legacy Notifications support",
      define => "ENABLE_LEGACY_NOTIFICATIONS", default => 0, value => \$legacyNotificationsSupport },

    { option => "legacy-vendor-prefixes", desc => "Toggle Legacy Vendor Prefix support",
      define => "ENABLE_LEGACY_VENDOR_PREFIXES", default => 1, value => \$legacyVendorPrefixSupport },

    { option => "legacy-web-audio", desc => "Toggle Legacy Web Audio support",
      define => "ENABLE_LEGACY_WEB_AUDIO", default => 1, value => \$legacyWebAudioSupport },

    { option => "link-prefetch", desc => "Toggle Link Prefetch support",
      define => "ENABLE_LINK_PREFETCH", default => (isGtk() || isEfl()), value => \$linkPrefetchSupport },

    { option => "jit", desc => "Enable just-in-time JavaScript support",
      define => "ENABLE_JIT", default => 1, value => \$jitSupport },

    { option => "mathml", desc => "Toggle MathML support",
      define => "ENABLE_MATHML", default => 1, value => \$mathmlSupport },

    { option => "media-capture", desc => "Toggle Media Capture support",
      define => "ENABLE_MEDIA_CAPTURE", default => isEfl(), value => \$mediaCaptureSupport },

    { option => "media-source", desc => "Toggle Media Source support",
      define => "ENABLE_MEDIA_SOURCE", default => (isGtk() || isEfl()), value => \$mediaSourceSupport },

    { option => "media-statistics", desc => "Toggle Media Statistics support",
      define => "ENABLE_MEDIA_STATISTICS", default => 0, value => \$mediaStatisticsSupport },

    { option => "media-stream", desc => "Toggle Media Stream support",
      define => "ENABLE_MEDIA_STREAM", default => 0, value => \$mediaStreamSupport },

    { option => "meter-element", desc => "Toggle Meter Element support",
      define => "ENABLE_METER_ELEMENT", default => !isAppleWinWebKit(), value => \$meterElementSupport },

    { option => "mhtml", desc => "Toggle MHTML support",
      define => "ENABLE_MHTML", default => (isGtk() || isEfl()), value => \$mhtmlSupport },

    { option => "mouse-cursor-scale", desc => "Toggle Scaled mouse cursor support",
      define => "ENABLE_MOUSE_CURSOR_SCALE", default => isEfl(), value => \$mouseCursorScaleSupport },

    { option => "navigator-content-utils", desc => "Toggle Navigator Content Utils support",
      define => "ENABLE_NAVIGATOR_CONTENT_UTILS", default => isEfl(), value => \$registerProtocolHandlerSupport },

    { option => "navigator-hardware-concurrency", desc => "Toggle Navigator hardware concurrenct support",
      define => "ENABLE_NAVIGATOR_HWCONCURRENCY", default => 1, value => \$hardwareConcurrencySupport },

    { option => "netscape-plugin-api", desc => "Toggle Netscape Plugin API support",
      define => "ENABLE_NETSCAPE_PLUGIN_API", default => !isIOSWebKit(), value => \$netscapePluginAPISupport },

    { option => "nosniff", desc => "Toggle support for 'X-Content-Type-Options: nosniff'",
      define => "ENABLE_NOSNIFF", default => isEfl(), value => \$nosniffSupport },

    { option => "notifications", desc => "Toggle Notifications support",
      define => "ENABLE_NOTIFICATIONS", default => isGtk(), value => \$notificationsSupport },

    { option => "orientation-events", desc => "Toggle Orientation Events support",
      define => "ENABLE_ORIENTATION_EVENTS", default => isIOSWebKit(), value => \$orientationEventsSupport },

    { option => "performance-timeline", desc => "Toggle Performance Timeline support",
      define => "ENABLE_PERFORMANCE_TIMELINE", default => isGtk(), value => \$performanceTimelineSupport },

    { option => "promises", desc => "Toggle Promise support",
      define => "ENABLE_PROMISES", default => 1, value => \$promiseSupport },

    { option => "proximity-events", desc => "Toggle Proximity Events support",
      define => "ENABLE_PROXIMITY_EVENTS", default => 0, value => \$proximityEventsSupport },

    { option => "quota", desc => "Toggle Quota support",
      define => "ENABLE_QUOTA", default => 0, value => \$quotaSupport },

    { option => "resolution-media-query", desc => "Toggle resolution media query support",
      define => "ENABLE_RESOLUTION_MEDIA_QUERY", default => isEfl(), value => \$resolutionMediaQuerySupport },

    { option => "resource-timing", desc => "Toggle Resource Timing support",
      define => "ENABLE_RESOURCE_TIMING", default => isGtk(), value => \$resourceTimingSupport },

    { option => "request-animation-frame", desc => "Toggle Request Animation Frame support",
      define => "ENABLE_REQUEST_ANIMATION_FRAME", default => (isAppleMacWebKit() || isGtk() || isEfl()), value => \$requestAnimationFrameSupport },

    { option => "seccomp-filters", desc => "Toggle Seccomp Filter sandbox",
      define => "ENABLE_SECCOMP_FILTERS", default => 0, value => \$seccompFiltersSupport },

    { option => "scripted-speech", desc => "Toggle Scripted Speech support",
      define => "ENABLE_SCRIPTED_SPEECH", default => 0, value => \$scriptedSpeechSupport },

    { option => "shadow-dom", desc => "Toggle Shadow DOM support",
      define => "ENABLE_SHADOW_DOM", default => (isAppleMacWebKit() || isIOSWebKit()), value => \$shadowDOMSupport },

    { option => "streams-api", desc => "Toggle Streams API support",
      define => "ENABLE_STREAMS_API", default => 1, value => \$streamsAPISupport },

    { option => "subtle-crypto", desc => "Toggle WebCrypto Subtle-Crypto support",
      define => "ENABLE_SUBTLE_CRYPTO", default => (isGtk() || isEfl() || isAppleMacWebKit() || isIOSWebKit()), value => \$subtleCrypto },

    { option => "svg-fonts", desc => "Toggle SVG Fonts support",
      define => "ENABLE_SVG_FONTS", default => 1, value => \$svgFontsSupport },

    { option => "system-malloc", desc => "Toggle system allocator instead of bmalloc",
      define => "USE_SYSTEM_MALLOC", default => 0, value => \$systemMallocSupport },

    { option => "template-element", desc => "Toggle HTMLTemplateElement support",
      define => "ENABLE_TEMPLATE_ELEMENT", default => 1, value => \$templateElementSupport },

    { option => "threaded-compositor", desc => "Toggle threaded compositor support",
      define => "ENABLE_THREADED_COMPOSITOR", default => 0, value => \$threadedCompositorSupport },

    { option => "text-autosizing", desc => "Toggle Text Autosizing support",
      define => "ENABLE_TEXT_AUTOSIZING", default => 0, value => \$textAutosizingSupport },

    { option => "touch-events", desc => "Toggle Touch Events support",
      define => "ENABLE_TOUCH_EVENTS", default => (isIOSWebKit() || isEfl() || isGtk()), value => \$touchEventsSupport },

    { option => "touch-slider", desc => "Toggle Touch Slider support",
      define => "ENABLE_TOUCH_SLIDER", default => isEfl(), value => \$touchSliderSupport },

    { option => "touch-icon-loading", desc => "Toggle Touch Icon Loading Support",
      define => "ENABLE_TOUCH_ICON_LOADING", default => 0, value => \$touchIconLoadingSupport },

    { option => "user-timing", desc => "Toggle User Timing support",
      define => "ENABLE_USER_TIMING", default => isGtk(), value => \$userTimingSupport },

    { option => "vibration", desc => "Toggle Vibration support",
      define => "ENABLE_VIBRATION", default => isEfl(), value => \$vibrationSupport },

    { option => "video", desc => "Toggle Video support",
      define => "ENABLE_VIDEO", default => (isAppleWebKit() || isGtk() || isEfl()), value => \$videoSupport },

    { option => "video-track", desc => "Toggle Video Track support",
      define => "ENABLE_VIDEO_TRACK", default => (isAppleWebKit() || isGtk() || isEfl()), value => \$videoTrackSupport },

    { option => "webgl", desc => "Toggle WebGL support",
      define => "ENABLE_WEBGL", default => (isAppleMacWebKit() || isIOSWebKit() || isGtk() || isEfl()), value => \$webglSupport },

    { option => "webassembly", desc => "Toggle WebAssembly support",
      define => "ENABLE_WEBASSEMBLY", default => 0, value => \$webAssemblySupport },

    { option => "web-animations", desc => "Toggle Web Animations support",
      define => "ENABLE_WEB_ANIMATIONS", default => 0, value => \$webAnimationsSupport },

    { option => "web-audio", desc => "Toggle Web Audio support",
      define => "ENABLE_WEB_AUDIO", default => (isEfl() || isGtk()), value => \$webAudioSupport },

    { option => "web-replay", desc => "Toggle Web Replay support",
      define => "ENABLE_WEB_REPLAY", default => isAppleMacWebKit(), value => \$webReplaySupport },

    { option => "web-sockets", desc => "Toggle Web Sockets support",
      define => "ENABLE_WEB_SOCKETS", default => 1, value => \$webSocketsSupport },

    { option => "web-timing", desc => "Toggle Web Timing support",
      define => "ENABLE_WEB_TIMING", default => (isGtk() || isEfl()), value => \$webTimingSupport },

    { option => "xslt", desc => "Toggle XSLT support",
      define => "ENABLE_XSLT", default => 1, value => \$xsltSupport },

    { option => "ftl-jit", desc => "Toggle FTLJIT support",
      define => "ENABLE_FTL_JIT", default => (isX86_64() && (isGtk() || isEfl())) , value => \$ftlJITSupport },
);

sub getFeatureOptionList()
{
    return @features;
}

1;
