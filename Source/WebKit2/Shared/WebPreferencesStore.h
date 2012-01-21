/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPreferencesStore_h
#define WebPreferencesStore_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// macro(KeyUpper, KeyLower, TypeNameUpper, TypeName, DefaultValue) 

#if PLATFORM(WIN)
#define DEFAULT_WEBKIT_AVFOUNDATION_ENABLED false
#else
#define DEFAULT_WEBKIT_AVFOUNDATION_ENABLED true
#endif

#if PLATFORM(GTK)
#define DEFAULT_WEBKIT_TABSTOLINKS_ENABLED true
#else
#define DEFAULT_WEBKIT_TABSTOLINKS_ENABLED false
#endif

#define FOR_EACH_WEBKIT_BOOL_PREFERENCE(macro) \
    macro(JavaScriptEnabled, javaScriptEnabled, Bool, bool, true) \
    macro(LoadsImagesAutomatically, loadsImagesAutomatically, Bool, bool, true) \
    macro(LoadsSiteIconsIgnoringImageLoadingPreference, loadsSiteIconsIgnoringImageLoadingPreference, Bool, bool, false) \
    macro(PluginsEnabled, pluginsEnabled, Bool, bool, true) \
    macro(JavaEnabled, javaEnabled, Bool, bool, true) \
    macro(OfflineWebApplicationCacheEnabled, offlineWebApplicationCacheEnabled, Bool, bool, false) \
    macro(LocalStorageEnabled, localStorageEnabled, Bool, bool, true) \
    macro(DatabasesEnabled, databasesEnabled, Bool, bool, true) \
    macro(XSSAuditorEnabled, xssAuditorEnabled, Bool, bool, true) \
    macro(FrameFlatteningEnabled, frameFlatteningEnabled, Bool, bool, false) \
    macro(DeveloperExtrasEnabled, developerExtrasEnabled, Bool, bool, false) \
    macro(PrivateBrowsingEnabled, privateBrowsingEnabled, Bool, bool, false) \
    macro(TextAreasAreResizable, textAreasAreResizable, Bool, bool, true) \
    macro(JavaScriptCanOpenWindowsAutomatically, javaScriptCanOpenWindowsAutomatically, Bool, bool, true) \
    macro(HyperlinkAuditingEnabled, hyperlinkAuditingEnabled, Bool, bool, true) \
    macro(NeedsSiteSpecificQuirks, needsSiteSpecificQuirks, Bool, bool, false) \
    macro(AcceleratedCompositingEnabled, acceleratedCompositingEnabled, Bool, bool, true) \
    macro(ForceCompositingMode, forceCompositingMode, Bool, bool, false) \
    macro(AcceleratedDrawingEnabled, acceleratedDrawingEnabled, Bool, bool, false) \
    macro(CanvasUsesAcceleratedDrawing, canvasUsesAcceleratedDrawing, Bool, bool, true) \
    macro(CompositingBordersVisible, compositingBordersVisible, Bool, bool, false) \
    macro(CompositingRepaintCountersVisible, compositingRepaintCountersVisible, Bool, bool, false) \
    macro(CSSCustomFilterEnabled, cssCustomFilterEnabled, Bool, bool, true) \
    macro(WebGLEnabled, webGLEnabled, Bool, bool, false) \
    macro(ForceFTPDirectoryListings, forceFTPDirectoryListings, Bool, bool, false) \
    macro(TabsToLinks, tabsToLinks, Bool, bool, DEFAULT_WEBKIT_TABSTOLINKS_ENABLED) \
    macro(DNSPrefetchingEnabled, dnsPrefetchingEnabled, Bool, bool, false) \
    macro(WebArchiveDebugModeEnabled, webArchiveDebugModeEnabled, Bool, bool, false) \
    macro(LocalFileContentSniffingEnabled, localFileContentSniffingEnabled, Bool, bool, false) \
    macro(UsesPageCache, usesPageCache, Bool, bool, true) \
    macro(PageCacheSupportsPlugins, pageCacheSupportsPlugins, Bool, bool, true) \
    macro(AuthorAndUserStylesEnabled, authorAndUserStylesEnabled, Bool, bool, true) \
    macro(PaginateDuringLayoutEnabled, paginateDuringLayoutEnabled, Bool, bool, false) \
    macro(DOMPasteAllowed, domPasteAllowed, Bool, bool, false) \
    macro(JavaScriptCanAccessClipboard, javaScriptCanAccessClipboard, Bool, bool, false) \
    macro(ShouldPrintBackgrounds, shouldPrintBackgrounds, Bool, bool, false) \
    macro(FullScreenEnabled, fullScreenEnabled, Bool, bool, false) \
    macro(WebSecurityEnabled, webSecurityEnabled, Bool, bool, true) \
    macro(AllowUniversalAccessFromFileURLs, allowUniversalAccessFromFileURLs, Bool, bool, false) \
    macro(AllowFileAccessFromFileURLs, allowFileAccessFromFileURLs, Bool, bool, false) \
    macro(AVFoundationEnabled, isAVFoundationEnabled, Bool, bool, DEFAULT_WEBKIT_AVFOUNDATION_ENABLED) \
    macro(Hixie76WebSocketProtocolEnabled, hixie76WebSocketProtocolEnabled, Bool, bool, true) \
    macro(MediaPlaybackRequiresUserGesture, mediaPlaybackRequiresUserGesture, Bool, bool, false) \
    macro(MediaPlaybackAllowsInline, mediaPlaybackAllowsInline, Bool, bool, true) \
    macro(InspectorStartsAttached, inspectorStartsAttached, Bool, bool, true) \
    macro(ShowsToolTipOverTruncatedText, showsToolTipOverTruncatedText, Bool, bool, false) \
    macro(MockScrollbarsEnabled, mockScrollbarsEnabled, Bool, bool, false) \
    macro(WebAudioEnabled, webAudioEnabled, Bool, bool, false) \
    macro(ApplicationChromeModeEnabled, applicationChromeMode, Bool, bool, false) \
    macro(SuppressIncrementalRendering, suppressIncrementalRendering, Bool, bool, false) \
    macro(BackspaceKeyNavigationEnabled, backspaceKeyNavigationEnabled, Bool, bool, true) \
    macro(CaretBrowsingEnabled, caretBrowsingEnabled, Bool, bool, false) \
    macro(ShouldDisplaySubtitles, shouldDisplaySubtitles, Bool, bool, false) \
    macro(ShouldDisplayCaptions, shouldDisplayCaptions, Bool, bool, false) \
    macro(ShouldDisplayTextDescriptions, shouldDisplayTextDescriptions, Bool, bool, false) \
    macro(NotificationsEnabled, notificationsEnabled, Bool, bool, true) \
    \

#define FOR_EACH_WEBKIT_DOUBLE_PREFERENCE(macro) \
    macro(PDFScaleFactor, pdfScaleFactor, Double, double, 0) \
    \

#if PLATFORM(WIN)
#define DEFAULT_WEBKIT_FONT_SMOOTHING_LEVEL FontSmoothingLevelWindows
#else
#define DEFAULT_WEBKIT_FONT_SMOOTHING_LEVEL FontSmoothingLevelMedium
#endif

#define FOR_EACH_WEBKIT_UINT32_PREFERENCE(macro) \
    macro(FontSmoothingLevel, fontSmoothingLevel, UInt32, uint32_t, DEFAULT_WEBKIT_FONT_SMOOTHING_LEVEL) \
    macro(MinimumFontSize, minimumFontSize, UInt32, uint32_t, 0) \
    macro(MinimumLogicalFontSize, minimumLogicalFontSize, UInt32, uint32_t, 9) \
    macro(DefaultFontSize, defaultFontSize, UInt32, uint32_t, 16) \
    macro(DefaultFixedFontSize, defaultFixedFontSize, UInt32, uint32_t, 13) \
    macro(LayoutFallbackWidth, layoutFallbackWidth, UInt32, uint32_t, 980) \
    macro(DeviceDPI, deviceDPI, UInt32, uint32_t, 240) \
    macro(DeviceWidth, deviceWidth, UInt32, uint32_t, 480) \
    macro(DeviceHeight, deviceHeight, UInt32, uint32_t, 854) \
    macro(PDFDisplayMode, pdfDisplayMode, UInt32, uint32_t, 1) \
    macro(EditableLinkBehavior, editableLinkBehavior, UInt32, uint32_t, WebCore::EditableLinkNeverLive) \
    macro(InspectorAttachedHeight, inspectorAttachedHeight, UInt32, uint32_t, 300) \
    \

#if PLATFORM(WIN)

#define FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(StandardFontFamily, standardFontFamily, String, String, "Times New Roman") \
    macro(CursiveFontFamily, cursiveFontFamily, String, String, "Comic Sans MS") \
    macro(FantasyFontFamily, fantasyFontFamily, String, String, "Comic Sans MS") \
    macro(FixedFontFamily, fixedFontFamily, String, String, "Courier New") \
    macro(SansSerifFontFamily, sansSerifFontFamily, String, String, "Arial") \
    macro(SerifFontFamily, serifFontFamily, String, String, "Times New Roman") \
    macro(PictographFontFamily, pictographFontFamily, String, String, "Times New Roman") \
    \

#elif PLATFORM(MAC)

#define FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(StandardFontFamily, standardFontFamily, String, String, "Times") \
    macro(CursiveFontFamily, cursiveFontFamily, String, String, "Apple Chancery") \
    macro(FantasyFontFamily, fantasyFontFamily, String, String, "Papyrus") \
    macro(FixedFontFamily, fixedFontFamily, String, String, "Courier") \
    macro(SansSerifFontFamily, sansSerifFontFamily, String, String, "Helvetica") \
    macro(SerifFontFamily, serifFontFamily, String, String, "Times") \
    macro(PictographFontFamily, pictographFontFamily, String, String, "Apple Color Emoji") \
    \

#elif PLATFORM(QT) || PLATFORM(GTK) || PLATFORM(EFL)

#define FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(StandardFontFamily, standardFontFamily, String, String, "Times") \
    macro(CursiveFontFamily, cursiveFontFamily, String, String, "Comic Sans MS") \
    macro(FantasyFontFamily, fantasyFontFamily, String, String, "Impact") \
    macro(FixedFontFamily, fixedFontFamily, String, String, "Courier New") \
    macro(SansSerifFontFamily, sansSerifFontFamily, String, String, "Helvetica") \
    macro(SerifFontFamily, serifFontFamily, String, String, "Times") \
    macro(PictographFontFamily, pictographFontFamily, String, String, "Times") \
    \

#endif

#define FOR_EACH_WEBKIT_STRING_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(DefaultTextEncodingName, defaultTextEncodingName, String, String, "ISO-8859-1") \
    macro(FTPDirectoryTemplatePath, ftpDirectoryTemplatePath, String, String, "") \
    \


#define FOR_EACH_WEBKIT_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_BOOL_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_DOUBLE_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_UINT32_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_STRING_PREFERENCE(macro) \
    \

namespace WebPreferencesKey {

#define DECLARE_KEY_GETTERS(KeyUpper, KeyLower, TypeName, Type, DefaultValue) const String& KeyLower##Key();

FOR_EACH_WEBKIT_PREFERENCE(DECLARE_KEY_GETTERS)

#undef DECLARE_KEY_GETTERS

} // namespace WebPreferencesKey

struct WebPreferencesStore {
    WebPreferencesStore();

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebPreferencesStore&);

    // NOTE: The getters in this class have non-standard names to aid in the use of the preference macros.

    bool setStringValueForKey(const String& key, const String& value);
    String getStringValueForKey(const String& key) const;

    bool setBoolValueForKey(const String& key, bool value);
    bool getBoolValueForKey(const String& key) const;

    bool setUInt32ValueForKey(const String& key, uint32_t value);
    uint32_t getUInt32ValueForKey(const String& key) const;

    bool setDoubleValueForKey(const String& key, double value);
    double getDoubleValueForKey(const String& key) const;

    static void overrideXSSAuditorEnabledForTestRunner(bool);
    static void removeTestRunnerOverrides();

    HashMap<String, String> m_stringValues;
    HashMap<String, bool> m_boolValues;
    HashMap<String, uint32_t> m_uint32Values;
    HashMap<String, double> m_doubleValues;
};

} // namespace WebKit

#endif // WebPreferencesStore_h
