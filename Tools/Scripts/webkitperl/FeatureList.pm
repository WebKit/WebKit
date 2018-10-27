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
    $accelerated2DCanvasSupport,
    $attachmentElementSupport,
    $bubblewrapSandboxSupport,
    $canvasProxySupport,
    $channelMessagingSupport,
    $css3TextSupport,
    $cssBoxDecorationBreakSupport,
    $cssCompositingSupport,
    $cssDeviceAdaptation,
    $cssGridLayoutSupport,
    $cssImageOrientationSupport,
    $cssImageResolutionSupport,
    $cssImageSetSupport,
    $cssShapesSupport,
    $customElementsSupport,
    $customSchemeHandlerSupport,
    $dataTransferItemsSupport,
    $darkModeCSSSupport,
    $datalistElementSupport,
    $deviceOrientationSupport,
    $dom4EventsConstructor,
    $downloadAttributeSupport,
    $encryptedMediaSupport,
    $fetchAPISupport,
    $fontLoadEventsSupport,
    $ftlJITSupport,
    $ftpDirSupport,
    $fullscreenAPISupport,
    $gamepadSupport,
    $geolocationSupport,
    $gstreamerGLSupport,
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
    $legacyEncryptedMediaSupport,
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
    $mouseCursorScaleSupport,
    $netscapePluginAPISupport,
    $notificationsSupport,
    $orientationEventsSupport,
    $performanceTimelineSupport,
    $proximityEventsSupport,
    $quotaSupport,
    $readableStreamAPISupport,
    $readableByteStreamAPISupport,
    $registerProtocolHandlerSupport,
    $resolutionMediaQuerySupport,
    $scriptedSpeechSupport,
    $serviceWorkerSupport,
    $subtleCrypto,
    $svgFontsSupport,
    $systemMallocSupport,
    $threadedCompositorSupport,
    $threeDTransformsSupport,
    $touchEventsSupport,
    $touchIconLoadingSupport,
    $touchSliderSupport,
    $videoSupport,
    $videoTrackSupport,
    $webAnimationsSupport,
    $webAssemblySupport,
    $webAudioSupport,
    $webAuthN,
    $webRTCSupport,
    $writableStreamAPISupport,
    $webglSupport,
    $webgl2Support,
    $xsltSupport,
);

prohibitUnknownPort();

my @features = (
    { option => "3d-rendering", desc => "Toggle 3D Rendering support",
      define => "ENABLE_3D_TRANSFORMS", value => \$threeDTransformsSupport },

    { option => "accelerated-2d-canvas", desc => "Toggle Accelerated 2D Canvas support",
      define => "ENABLE_ACCELERATED_2D_CANVAS", value => \$accelerated2DCanvasSupport },

    { option => "attachment-element", desc => "Toggle Attachment Element support",
      define => "ENABLE_ATTACHMENT_ELEMENT", value => \$attachmentElementSupport },

    { option => "bubblewrap-sandbox", desc => "Toggle Bubblewrap sandboxing support",
      define => "ENABLE_BUBBLEWRAP_SANDBOX", value => \$bubblewrapSandboxSupport },

    { option => "channel-messaging", desc => "Toggle Channel Messaging support",
      define => "ENABLE_CHANNEL_MESSAGING", value => \$channelMessagingSupport },

    { option => "css-box-decoration-break", desc => "Toggle CSS box-decoration-break support",
      define => "ENABLE_CSS_BOX_DECORATION_BREAK", value => \$cssBoxDecorationBreakSupport },

    { option => "css-compositing", desc => "Toggle CSS Compositing support",
      define => "ENABLE_CSS_COMPOSITING", value => \$cssCompositingSupport },

    { option => "css-device-adaptation", desc => "Toggle CSS Device Adaptation support",
      define => "ENABLE_CSS_DEVICE_ADAPTATION", value => \$cssDeviceAdaptation },

    { option => "css-image-orientation", desc => "Toggle CSS image-orientation support",
      define => "ENABLE_CSS_IMAGE_ORIENTATION", value => \$cssImageOrientationSupport },

    { option => "css-image-resolution", desc => "Toggle CSS image-resolution support",
      define => "ENABLE_CSS_IMAGE_RESOLUTION", value => \$cssImageResolutionSupport },

    { option => "css-image-set", desc => "Toggle CSS image-set support",
      define => "ENABLE_CSS_IMAGE_SET", value => \$cssImageSetSupport },

    { option => "css-shapes", desc => "Toggle CSS Shapes support",
      define => "ENABLE_CSS_SHAPES", value => \$cssShapesSupport },

    { option => "css3-text", desc => "Toggle CSS3 Text support",
      define => "ENABLE_CSS3_TEXT", value => \$css3TextSupport },

    { option => "custom-elements", desc => "Toggle custom elements support",
      define => "ENABLE_CUSTOM_ELEMENTS", value => \$customElementsSupport },

    { option => "custom-scheme-handler", desc => "Toggle Custom Scheme Handler support",
      define => "ENABLE_CUSTOM_SCHEME_HANDLER", value => \$customSchemeHandlerSupport },

    { option => "dark-mode-css", desc => "Toggle Dark Mode CSS support",
      define => "ENABLE_DARK_MODE_CSS", value => \$darkModeCSSSupport },

    { option => "data-transfer-items", desc => "Toggle Data Transfer Items support",
      define => "ENABLE_DATA_TRANSFER_ITEMS", value => \$dataTransferItemsSupport },

    { option => "datalist-element", desc => "Toggle Datalist Element support",
      define => "ENABLE_DATALIST_ELEMENT", value => \$datalistElementSupport },

    { option => "device-orientation", desc => "Toggle Device Orientation support",
      define => "ENABLE_DEVICE_ORIENTATION", value => \$deviceOrientationSupport },

    { option => "dom4-events-constructor", desc => "Expose DOM4 Events constructors",
      define => "ENABLE_DOM4_EVENTS_CONSTRUCTOR", value => \$dom4EventsConstructor },

    { option => "download-attribute", desc => "Toggle Download Attribute support",
      define => "ENABLE_DOWNLOAD_ATTRIBUTE", value => \$downloadAttributeSupport },

    { option => "encrypted-media", desc => "Toggle EME V3 support",
      define => "ENABLE_ENCRYPTED_MEDIA", value => \$encryptedMediaSupport },

    { option => "fetch-api", desc => "Toggle Fetch API support",
      define => "ENABLE_FETCH_API", value => \$fetchAPISupport },

    { option => "font-load-events", desc => "Toggle Font Load Events support",
      define => "ENABLE_FONT_LOAD_EVENTS", value => \$fontLoadEventsSupport },

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

    { option => "gstreamer-gl", desc => "Toggle GStreamer GL support",
      define => "USE_GSTREAMER_GL", value => \$gstreamerGLSupport },

    { option => "high-dpi-canvas", desc => "Toggle High DPI Canvas support",
      define => "ENABLE_HIGH_DPI_CANVAS", value => \$highDPICanvasSupport },

    { option => "icon-database", desc => "Toggle Icondatabase support",
      define => "ENABLE_ICONDATABASE", value => \$icondatabaseSupport },

    { option => "indexed-database", desc => "Toggle Indexed Database support",
      define => "ENABLE_INDEXED_DATABASE", value => \$indexedDatabaseSupport },

    { option => "input-speech", desc => "Toggle Input Speech support",
      define => "ENABLE_INPUT_SPEECH", value => \$inputSpeechSupport },

    { option => "input-type-color", desc => "Toggle Input Type Color support",
      define => "ENABLE_INPUT_TYPE_COLOR", value => \$inputTypeColorSupport },

    { option => "input-type-date", desc => "Toggle Input Type Date support",
      define => "ENABLE_INPUT_TYPE_DATE", value => \$inputTypeDateSupport },

    { option => "input-type-datetime", desc => "Toggle broken Input Type Datetime support",
      define => "ENABLE_INPUT_TYPE_DATETIME_INCOMPLETE", value => \$inputTypeDatetimeSupport },

    { option => "input-type-datetimelocal", desc => "Toggle Input Type Datetimelocal support",
      define => "ENABLE_INPUT_TYPE_DATETIMELOCAL", value => \$inputTypeDatetimelocalSupport },

    { option => "input-type-month", desc => "Toggle Input Type Month support",
      define => "ENABLE_INPUT_TYPE_MONTH", value => \$inputTypeMonthSupport },

    { option => "input-type-time", desc => "Toggle Input Type Time support",
      define => "ENABLE_INPUT_TYPE_TIME", value => \$inputTypeTimeSupport },

    { option => "input-type-week", desc => "Toggle Input Type Week support",
      define => "ENABLE_INPUT_TYPE_WEEK", value => \$inputTypeWeekSupport },

    { option => "intl", desc => "Toggle Intl support",
      define => "ENABLE_INTL", value => \$intlSupport },

    { option => "jit", desc => "Enable just-in-time JavaScript support",
      define => "ENABLE_JIT", value => \$jitSupport },

    { option => "legacy-encrypted-media", desc => "Toggle Legacy EME V2 support",
      define => "ENABLE_LEGACY_ENCRYPTED_MEDIA", value => \$legacyEncryptedMediaSupport },

    { option => "legacy-web-audio", desc => "Toggle Legacy Web Audio support",
      define => "ENABLE_LEGACY_WEB_AUDIO", value => \$legacyWebAudioSupport },

    { option => "link-prefetch", desc => "Toggle Link Prefetch support",
      define => "ENABLE_LINK_PREFETCH", value => \$linkPrefetchSupport },

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

    { option => "meter-element", desc => "Toggle Meter Element support",
      define => "ENABLE_METER_ELEMENT", value => \$meterElementSupport },

    { option => "mhtml", desc => "Toggle MHTML support",
      define => "ENABLE_MHTML", value => \$mhtmlSupport },

    { option => "mouse-cursor-scale", desc => "Toggle Scaled mouse cursor support",
      define => "ENABLE_MOUSE_CURSOR_SCALE", value => \$mouseCursorScaleSupport },

    { option => "navigator-content-utils", desc => "Toggle Navigator Content Utils support",
      define => "ENABLE_NAVIGATOR_CONTENT_UTILS", value => \$registerProtocolHandlerSupport },

    { option => "navigator-hardware-concurrency", desc => "Toggle Navigator hardware concurrency support",
      define => "ENABLE_NAVIGATOR_HWCONCURRENCY", value => \$hardwareConcurrencySupport },

    { option => "netscape-plugin-api", desc => "Toggle Netscape Plugin API support",
      define => "ENABLE_NETSCAPE_PLUGIN_API", value => \$netscapePluginAPISupport },

    { option => "notifications", desc => "Toggle Notifications support",
      define => "ENABLE_NOTIFICATIONS", value => \$notificationsSupport },

    { option => "orientation-events", desc => "Toggle Orientation Events support",
      define => "ENABLE_ORIENTATION_EVENTS", value => \$orientationEventsSupport },

    { option => "performance-timeline", desc => "Toggle Performance Timeline support",
      define => "ENABLE_PERFORMANCE_TIMELINE", value => \$performanceTimelineSupport },

    { option => "proximity-events", desc => "Toggle Proximity Events support",
      define => "ENABLE_PROXIMITY_EVENTS", value => \$proximityEventsSupport },

    { option => "quota", desc => "Toggle Quota support",
      define => "ENABLE_QUOTA", value => \$quotaSupport },

    { option => "readableStreamAPI", desc => "Toggle ReadableStream API support",
      define => "ENABLE_READABLE_STREAM_API", value => \$readableStreamAPISupport },

    { option => "readableByteStreamAPI", desc => "Toggle support of ByteStream part of ReadableStream API",
      define => "ENABLE_READABLE_BYTE_STREAM_API", value => \$readableByteStreamAPISupport },

    { option => "resolution-media-query", desc => "Toggle resolution media query support",
      define => "ENABLE_RESOLUTION_MEDIA_QUERY", value => \$resolutionMediaQuerySupport },

    { option => "scripted-speech", desc => "Toggle Scripted Speech support",
      define => "ENABLE_SCRIPTED_SPEECH", value => \$scriptedSpeechSupport },

    { option => "service-worker", desc => "Toggle Service Worker support",
      define => "ENABLE_SERVICE_WORKER", value => \$serviceWorkerSupport },

    { option => "subtle-crypto", desc => "Toggle WebCrypto Subtle-Crypto support",
      define => "ENABLE_SUBTLE_CRYPTO", value => \$subtleCrypto },

    { option => "svg-fonts", desc => "Toggle SVG Fonts support",
      define => "ENABLE_SVG_FONTS", value => \$svgFontsSupport },

    { option => "system-malloc", desc => "Toggle system allocator instead of bmalloc",
      define => "USE_SYSTEM_MALLOC", value => \$systemMallocSupport },

    { option => "touch-events", desc => "Toggle Touch Events support",
      define => "ENABLE_TOUCH_EVENTS", value => \$touchEventsSupport },

    { option => "touch-slider", desc => "Toggle Touch Slider support",
      define => "ENABLE_TOUCH_SLIDER", value => \$touchSliderSupport },

    { option => "video", desc => "Toggle Video support",
      define => "ENABLE_VIDEO", value => \$videoSupport },

    { option => "video-track", desc => "Toggle Video Track support",
      define => "ENABLE_VIDEO_TRACK", value => \$videoTrackSupport },

    { option => "web-animations", desc => "Toggle Web Animations support",
      define => "ENABLE_WEB_ANIMATIONS", value => \$webAnimationsSupport },

    { option => "web-audio", desc => "Toggle Web Audio support",
      define => "ENABLE_WEB_AUDIO", value => \$webAudioSupport },

    { option => "web-authn", desc => "Toggle Web Authn support",
      define => "ENABLE_WEB_AUTHN", value => \$webAuthN },

    { option => "web-rtc", desc => "Toggle WebRTC support",
      define => "ENABLE_WEB_RTC", value => \$webRTCSupport },

    { option => "webassembly", desc => "Toggle WebAssembly support",
      define => "ENABLE_WEBASSEMBLY", value => \$webAssemblySupport },

    { option => "webgl", desc => "Toggle WebGL support",
      define => "ENABLE_WEBGL", value => \$webglSupport },

    { option => "webgl2", desc => "Toggle WebGL2 support",
      define => "ENABLE_WEBGL2", value => \$webgl2Support },

    { option => "writableStreamAPI", desc => "Toggle WritableStream API support",
      define => "ENABLE_WRITABLE_STREAM_API", value => \$writableStreamAPISupport },

    { option => "xslt", desc => "Toggle XSLT support",
      define => "ENABLE_XSLT", value => \$xsltSupport },
);

sub getFeatureOptionList()
{
    return @features;
}

1;
