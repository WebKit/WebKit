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
    $requestAnimationFrameSupport,
    $threeDCanvasSupport,
    $threeDRenderingSupport,
    $accelerated2dCanvasSupport,
    $animationAPISupport,
    $batterystatusAPISupport,
    $blobSupport,
    $channelMessagingSupport,
    $cssFiltersSupport,
    $cssGridLayoutSupport,
    $cssShadersSupport,
    $sqlDatabaseSupport,
    $datalistSupport,
    $dataTransferItemsSupport,
    $detailsSupport,
    $deviceOrientationSupport,
    $directoryUploadSupport,
    $downloadAttributeSupport,
    $fileSystemSupport,
    $filtersSupport,
    $ftpDirSupport,
    $fullscreenAPISupport,
    $gamepadSupport,
    $geolocationSupport,
    $highDPICanvasSupport,
    $iconDatabaseSupport,
    $imageResizerSupport,
    $indexedDatabaseSupport,
    $inputTypeColorSupport,
    $inputSpeechSupport,
    $scriptedSpeechSupport,
    $inputTypeDateSupport,
    $inputTypeDatetimeSupport,
    $inputTypeDatetimelocalSupport,
    $inputTypeMonthSupport,
    $inputTypeTimeSupport,
    $inputTypeWeekSupport,
    $javaScriptDebuggerSupport,
    $legacyNotificationsSupport,
    $legacyWebKitBlobBuilderSupport,
    $linkPrefetchSupport,
    $linkPrerenderSupport,
    $mathmlSupport,
    $mediaSourceSupport,
    $mediaStatisticsSupport,
    $mediaStreamSupport,
    $meterTagSupport,
    $mhtmlSupport,
    $microdataSupport,
    $mutationObserversSupport,
    $netscapePluginSupport,
    $networkInfoSupport,
    $notificationsSupport,
    $orientationEventsSupport,
    $pageVisibilityApiSupport,
    $progressTagSupport,
    $quotaSupport,
    $registerProtocolHandlerSupport,
    $shadowDomSupport,
    $sharedWorkersSupport,
    $styleScopedSupport,
    $svgSupport,
    $svgDOMObjCBindingsSupport,
    $svgFontsSupport,
    $systemMallocSupport,
    $tiledBackingStoreSupport,
    $touchEventsSupport,
    $touchIconLoadingSupport,
    $vibrationSupport,
    $videoSupport,
    $videoTrackSupport,
    $webAudioSupport,
    $webInspectorSupport,
    $webSocketsSupport,
    $webTimingSupport,
    $workersSupport,
    $xsltSupport,
);

my @features = (
    { option => "request-animation-frame", desc => "Toggle requestAnimationFrame support",
      define => "ENABLE_REQUEST_ANIMATION_FRAME", default => (isAppleMacWebKit() || isGtk() || isEfl() || isBlackBerry()), value => \$requestAnimationFrameSupport },

    { option => "download-attribute", desc => "Toggle download attribute support",
      define => "ENABLE_DOWNLOAD_ATTRIBUTE", default => isBlackBerry(), value =>\$downloadAttributeSupport },

    { option => "3d-canvas", desc => "Toggle 3D canvas (WebGL) support",
      define => "ENABLE_WEBGL", default => isAppleMacWebKit(), value => \$threeDCanvasSupport },

    { option => "3d-rendering", desc => "Toggle 3D rendering support",
      define => "ENABLE_3D_RENDERING", default => (isAppleMacWebKit() || isQt()), value => \$threeDRenderingSupport },

    { option => "accelerated-2d-canvas", desc => "Toggle accelerated 2D canvas support",
      define => "ENABLE_ACCELERATED_2D_CANVAS", default => 0, value => \$accelerated2dCanvasSupport },

    { option => "animation-api", desc => "Toggle animation API support",
      define => "ENABLE_ANIMATION_API", default => isBlackBerry(), value => \$animationAPISupport },

    { option => "battery-status-api", desc => "Toggle battery status API support",
      define => "ENABLE_BATTERY_STATUS", default => (isEfl() || isBlackBerry()), value => \$batterystatusAPISupport },

    { option => "blob", desc => "Toggle Blob support",
      define => "ENABLE_BLOB", default => (isAppleMacWebKit() || isGtk() || isChromium() || isBlackBerry()), value => \$blobSupport },

    { option => "channel-messaging", desc => "Toggle MessageChannel and MessagePort support",
      define => "ENABLE_CHANNEL_MESSAGING", default => 1, value => \$channelMessagingSupport },

    { option => "css-filters", desc => "Toggle CSS Filters support",
      define => "ENABLE_CSS_FILTERS", default => isAppleWebKit(), value => \$cssFiltersSupport },

    { option => "css-grid-layout", desc => "Toggle CSS Grid Layout support",
      define => "ENABLE_CSS_GRID_LAYOUT", default => 0, value => \$cssGridLayoutSupport },

    { option => "css-shaders", desc => "Toggle CSS Shaders (within CSS Filters) support",
      define => "ENABLE_CSS_SHADERS", default => 0, value => \$cssShadersSupport },

    { option => "sql-database", desc => "Toggle SQL Database Support",
      define => "ENABLE_SQL_DATABASE", default => 1, value => \$sqlDatabaseSupport },

    { option => "datalist", desc => "Toggle HTML5 datalist support",
      define => "ENABLE_DATALIST", default => 0, value => \$datalistSupport },

    { option => "data-transfer-items", desc => "Toggle HTML5 data transfer items support",
      define => "ENABLE_DATA_TRANSFER_ITEMS", default => 0, value => \$dataTransferItemsSupport },

    { option => "details", desc => "Toggle HTML5 details support",
      define => "ENABLE_DETAILS", default => 1, value => \$detailsSupport },

    { option => "device-orientation", desc => "Toggle DeviceOrientation support",
      define => "ENABLE_DEVICE_ORIENTATION", default => isBlackBerry(), value => \$deviceOrientationSupport },

    { option => "directory-upload", desc => "Toogle Directory upload support",
      define => "ENABLE_DIRECTORY_UPLOAD", default => 0, value => \$directoryUploadSupport },

    { option => "file-system", desc => "Toggle FileSystem support",
      define => "ENABLE_FILE_SYSTEM", default => isBlackBerry(), value => \$fileSystemSupport },

    { option => "filters", desc => "Toggle SVG Filters support",
      define => "ENABLE_FILTERS", default => (isAppleWebKit() || isGtk() || isQt() || isEfl() || isBlackBerry()), value => \$filtersSupport },

    { option => "ftpdir", desc => "Toggle FTP directory support",
      define => "ENABLE_FTPDIR", default => !isWinCE(), value => \$ftpDirSupport },

    { option => "fullscreen-api", desc => "Toggle Fullscreen API support",
      define => "ENABLE_FULLSCREEN_API", default => (isAppleMacWebKit() || isGtk() || isBlackBerry()), value => \$fullscreenAPISupport },

    { option => "gamepad", desc => "Toggle Gamepad support",
      define => "ENABLE_GAMEPAD", default => 0, value => \$gamepadSupport },

    { option => "geolocation", desc => "Toggle Geolocation support",
      define => "ENABLE_GEOLOCATION", default => (isAppleWebKit() || isGtk() || isBlackBerry()), value => \$geolocationSupport },

    { option => "high-dpi-canvas", desc => "Toggle high-DPI canvas backing store support",
      define => "ENABLE_HIGH_DPI_CANVAS", default => (isAppleWebKit()), value => \$highDPICanvasSupport },

    { option => "icon-database", desc => "Toggle Icon database support",
      define => "ENABLE_ICONDATABASE", default => 1, value => \$iconDatabaseSupport },

    { option => "indexed-database", desc => "Toggle Indexed Database API support",
      define => "ENABLE_INDEXED_DATABASE", default => 0, value => \$indexedDatabaseSupport },

    { option => "input-speech", desc => "Speech Input API support",
      define => "ENABLE_INPUT_SPEECH", default => 0, value => \$inputSpeechSupport },

    { option => "scripted-speech", desc => "Scripted Speech API support",
      define => "ENABLE_SCRIPTED_SPEECH", default => 0, value => \$scriptedSpeechSupport },

    { option => "input-type-color", desc => "Toggle color type <input> support",
      define => "ENABLE_INPUT_TYPE_COLOR", default => (isBlackBerry() || isEfl()), value => \$inputTypeColorSupport },

    { option => "input-type-date", desc => "Toggle date type <input> support",
      define => "ENABLE_INPUT_TYPE_DATE", default => 0, value => \$inputTypeDateSupport },

    { option => "input-type-datetime", desc => "Toggle datetime type <input> support",
      define => "ENABLE_INPUT_TYPE_DATETIME", default => 0, value => \$inputTypeDatetimeSupport },

    { option => "input-type-datetimelocal", desc => "Toggle datetime-local type <input> support",
      define => "ENABLE_INPUT_TYPE_DATETIMELOCAL", default => 0, value => \$inputTypeDatetimelocalSupport },

    { option => "input-type-month", desc => "Toggle month type <input> support",
      define => "ENABLE_INPUT_TYPE_MONTH", default => 0, value => \$inputTypeMonthSupport },

    { option => "input-type-time", desc => "Toggle time type <input> support",
      define => "ENABLE_INPUT_TYPE_TIME", default => 0, value => \$inputTypeTimeSupport },

    { option => "input-type-week", desc => "Toggle week type <input> support",
      define => "ENABLE_INPUT_TYPE_WEEK", default => 0, value => \$inputTypeWeekSupport },

    { option => "inspector", desc => "Toggle Web Inspector support",
      define => "ENABLE_INSPECTOR", default => !isWinCE(), value => \$webInspectorSupport },

    { option => "javascript-debugger", desc => "Toggle JavaScript Debugger/Profiler support",
      define => "ENABLE_JAVASCRIPT_DEBUGGER", default => 1, value => \$javaScriptDebuggerSupport },

    { option => "legacy-notifications", desc => "Toggle Legacy Desktop Notifications Support",
      define => "ENABLE_LEGACY_NOTIFICATIONS", default => isBlackBerry(), value => \$legacyNotificationsSupport },

    { option => "legacy-webkit-blob-builder", desc => "Toggle Legacy WebKitBlobBuilder Support",
      define => "ENABLE_LEGACY_WEBKIT_BLOB_BUILDER", default => (isGtk() || isChromium() || isBlackBerry()), value => \$legacyWebKitBlobBuilderSupport },

    { option => "link-prefetch", desc => "Toggle pre fetching support",
      define => "ENABLE_LINK_PREFETCH", default => 0, value => \$linkPrefetchSupport },

    { option => "link-prerender", desc => "Toggle pre rendering support",
      define => "ENABLE_LINK_PRERENDER", default => 0, value => \$linkPrerenderSupport },

    { option => "mathml", desc => "Toggle MathML support",
      define => "ENABLE_MATHML", default => 1, value => \$mathmlSupport },

    { option => "media-source", desc => "Toggle Media Source support",
      define => "ENABLE_MEDIA_SOURCE", default => 0, value => \$mediaSourceSupport },

    { option => "media-statistics", desc => "Toggle Media Statistics support",
      define => "ENABLE_MEDIA_STATISTICS", default => 0, value => \$mediaStatisticsSupport },

    { option => "media-stream", desc => "Toggle Media Stream API support (implies Blob support, currently Chromium and GTK only)",
      define => "ENABLE_MEDIA_STREAM", default => (isChromium() || isGtk()), value => \$mediaStreamSupport },

    { option => "meter-tag", desc => "Meter Tag support",
      define => "ENABLE_METER_TAG", default => !isAppleWinWebKit(), value => \$meterTagSupport },

    { option => "mhtml", desc => "Toggle MHTML support",
      define => "ENABLE_MHTML", default => 0, value => \$mhtmlSupport },

    { option => "microdata", desc => "Toggle Microdata support",
      define => "ENABLE_MICRODATA", default => 0, value => \$microdataSupport },

    { option => "mutation-observers", desc => "Toggle DOM mutation observer support",
      define => "ENABLE_MUTATION_OBSERVERS", default => 1, value => \$mutationObserversSupport },

    { option => "netscape-plugin", desc => "Netscape Plugin support",
      define => "ENABLE_NETSCAPE_PLUGIN_API", default => !isEfl(), value => \$netscapePluginSupport },

    { option => "network-info", desc => "Network Information API support",
      define => "ENABLE_NETWORK_INFO", default => isEfl(), value => \$networkInfoSupport },

    { option => "notifications", desc => "Toggle Desktop Notifications Support",
      define => "ENABLE_NOTIFICATIONS", default => isBlackBerry(), value => \$notificationsSupport },

    { option => "orientation-events", desc => "Toggle Orientation Events support",
      define => "ENABLE_ORIENTATION_EVENTS", default => isBlackBerry(), value => \$orientationEventsSupport },

    { option => "page-visibility-api", desc => "Page Visibility API support",
      define => "ENABLE_PAGE_VISIBILITY_API", default => (isBlackBerry() || isEfl()), value => \$pageVisibilityApiSupport },

    { option => "progress-tag", desc => "Progress Tag support",
      define => "ENABLE_PROGRESS_TAG", default => 1, value => \$progressTagSupport },

    { option => "quota", desc => "Toggle Quota support",
      define => "ENABLE_QUOTA", default => 0, value => \$quotaSupport },

    { option => "register-protocol-handler", desc => "Register Protocol Handler support",
      define => "ENABLE_REGISTER_PROTOCOL_HANDLER", default => 0, value => \$registerProtocolHandlerSupport },

    { option => "system-malloc", desc => "Toggle system allocator instead of TCmalloc",
      define => "USE_SYSTEM_MALLOC", default => isWinCE(), value => \$systemMallocSupport },

    { option => "shadow-dom", desc => "Toggle Shadow DOM support",
      define => "ENABLE_SHADOW_DOM", default => isGtk(), value => \$shadowDomSupport },

    { option => "shared-workers", desc => "Toggle SharedWorkers support",
      define => "ENABLE_SHARED_WORKERS", default => (isAppleWebKit() || isGtk() || isBlackBerry() || isEfl()), value => \$sharedWorkersSupport },

    { option => "style-scoped", desc => "Toggle <style scoped> support",
      define => "ENABLE_STYLE_SCOPED", default => 0, value => \$styleScopedSupport },

    { option => "svg", desc => "Toggle SVG support",
      define => "ENABLE_SVG", default => 1, value => \$svgSupport },

    { option => "svg-dom-objc-bindings", desc => "Toggle SVG DOM Objective-C bindings support (implies SVG support)",
      define => "ENABLE_SVG_DOM_OBJC_BINDINGS", default => isAppleMacWebKit(), value => \$svgDOMObjCBindingsSupport },

    { option => "svg-fonts", desc => "Toggle SVG fonts support (imples SVG support)",
      define => "ENABLE_SVG_FONTS", default => 1, value => \$svgFontsSupport },

    { option => "tiled-backing-store", desc => "Toggle Tiled Backing Store support",
      define => "WTF_USE_TILED_BACKING_STORE", default => isQt(), value => \$tiledBackingStoreSupport },

    { option => "touch-events", desc => "Toggle Touch Events support",
      define => "ENABLE_TOUCH_EVENTS", default => (isQt() || isBlackBerry()), value => \$touchEventsSupport },

    { option => "touch-icon-loading", desc => "Toggle Touch Icon Loading Support",
      define => "ENABLE_TOUCH_ICON_LOADING", default => 0, value => \$touchIconLoadingSupport },

    { option => "vibration", desc => "Toggle Vibration API support",
      define => "ENABLE_VIBRATION", default => (isEfl() || isBlackBerry()), value => \$vibrationSupport },

    { option => "video", desc => "Toggle Video support",
      define => "ENABLE_VIDEO", default => (isAppleWebKit() || isGtk() || isBlackBerry() || isEfl()), value => \$videoSupport },

    { option => "video-track", desc => "Toggle Video Track support",
      define => "ENABLE_VIDEO_TRACK", default => (isAppleWebKit() || isGtk() || isEfl()), value => \$videoTrackSupport },

    { option => "web-audio", desc => "Toggle Web Audio support",
      define => "ENABLE_WEB_AUDIO", default => 0, value=> \$webAudioSupport },

    { option => "web-sockets", desc => "Toggle Web Sockets support",
      define => "ENABLE_WEB_SOCKETS", default => 1, value=> \$webSocketsSupport },

    { option => "web-timing", desc => "Toggle Web Timing support",
      define => "ENABLE_WEB_TIMING", default => (isBlackBerry() || isGtk() || isEfl()), value=> \$webTimingSupport },

    { option => "workers", desc => "Toggle Web Workers support",
      define => "ENABLE_WORKERS", default => (isAppleWebKit() || isGtk() || isBlackBerry() || isEfl()), value => \$workersSupport },

    { option => "xslt", desc => "Toggle XSLT support",
      define => "ENABLE_XSLT", default => 1, value => \$xsltSupport },
);

sub getFeatureOptionList()
{
    return @features;
}

1;
