/*
 * Copyright (C) 2006-2009, 2015 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "WebKit.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/BString.h>
#include <wtf/RetainPtr.h>

class WebPreferences final : public IWebPreferences, public IWebPreferencesPrivate7 {
public:
    static WebPreferences* createInstance();
protected:
    WebPreferences();
    ~WebPreferences();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebPreferences
    virtual HRESULT STDMETHODCALLTYPE standardPreferences(_COM_Outptr_opt_ IWebPreferences**);
    virtual HRESULT STDMETHODCALLTYPE initWithIdentifier(_In_ BSTR, _COM_Outptr_opt_ IWebPreferences**);
    virtual HRESULT STDMETHODCALLTYPE identifier(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE standardFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setStandardFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE fixedFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setFixedFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE serifFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setSerifFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE sansSerifFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setSansSerifFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE cursiveFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setCursiveFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE fantasyFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setFantasyFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE pictographFontFamily(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setPictographFontFamily(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE defaultFontSize(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultFontSize(int);
    virtual HRESULT STDMETHODCALLTYPE defaultFixedFontSize(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultFixedFontSize(int);
    virtual HRESULT STDMETHODCALLTYPE minimumFontSize(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setMinimumFontSize(int);
    virtual HRESULT STDMETHODCALLTYPE minimumLogicalFontSize(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setMinimumLogicalFontSize(int);
    virtual HRESULT STDMETHODCALLTYPE defaultTextEncodingName(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultTextEncodingName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE userStyleSheetEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setUserStyleSheetEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE userStyleSheetLocation(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setUserStyleSheetLocation(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE isJavaEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setJavaEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isJavaScriptEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE javaScriptCanOpenWindowsAutomatically(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptCanOpenWindowsAutomatically(BOOL);
    virtual HRESULT STDMETHODCALLTYPE arePlugInsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setPlugInsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isCSSRegionsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCSSRegionsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE unused7();
    virtual HRESULT STDMETHODCALLTYPE unused8();
    virtual HRESULT STDMETHODCALLTYPE allowsAnimatedImages(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowsAnimatedImages(BOOL);
    virtual HRESULT STDMETHODCALLTYPE allowAnimatedImageLooping(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowAnimatedImageLooping(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setLoadsImagesAutomatically(BOOL);
    virtual HRESULT STDMETHODCALLTYPE loadsImagesAutomatically(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAutosaves(BOOL);
    virtual HRESULT STDMETHODCALLTYPE autosaves(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldPrintBackgrounds(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldPrintBackgrounds(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setPrivateBrowsingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE privateBrowsingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setTabsToLinks(BOOL);
    virtual HRESULT STDMETHODCALLTYPE tabsToLinks(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE textAreasAreResizable(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setTextAreasAreResizable(BOOL);
    virtual HRESULT STDMETHODCALLTYPE usesPageCache(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setUsesPageCache(BOOL);
    virtual HRESULT STDMETHODCALLTYPE unused1();
    virtual HRESULT STDMETHODCALLTYPE unused2();
    virtual HRESULT STDMETHODCALLTYPE iconDatabaseLocation(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setIconDatabaseLocation(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE iconDatabaseEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setIconDatabaseEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE fontSmoothing(_Out_ FontSmoothingType*);
    virtual HRESULT STDMETHODCALLTYPE setFontSmoothing(FontSmoothingType);
    virtual HRESULT STDMETHODCALLTYPE editableLinkBehavior(_Out_ WebKitEditableLinkBehavior*);
    virtual HRESULT STDMETHODCALLTYPE setEditableLinkBehavior(WebKitEditableLinkBehavior);
    virtual HRESULT STDMETHODCALLTYPE unused5();
    virtual HRESULT STDMETHODCALLTYPE unused6();
    virtual HRESULT STDMETHODCALLTYPE cookieStorageAcceptPolicy(_Out_ WebKitCookieStorageAcceptPolicy*);
    virtual HRESULT STDMETHODCALLTYPE setCookieStorageAcceptPolicy(WebKitCookieStorageAcceptPolicy);
    virtual HRESULT STDMETHODCALLTYPE continuousSpellCheckingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setContinuousSpellCheckingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE grammarCheckingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setGrammarCheckingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE allowContinuousSpellChecking(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowContinuousSpellChecking(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isDOMPasteAllowed(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDOMPasteAllowed(BOOL);
    virtual HRESULT STDMETHODCALLTYPE cacheModel(_Out_ WebCacheModel*);
    virtual HRESULT STDMETHODCALLTYPE setCacheModel(WebCacheModel);
    virtual HRESULT STDMETHODCALLTYPE unused3();
    virtual HRESULT STDMETHODCALLTYPE unused4();
    virtual HRESULT STDMETHODCALLTYPE setAVFoundationEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE avFoundationEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldDisplaySubtitles(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldDisplaySubtitles(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldDisplayCaptions(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldDisplayCaptions(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldDisplayTextDescriptions(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldDisplayTextDescriptions(_Out_ BOOL*);

    // IWebPreferencesPrivate
    virtual HRESULT STDMETHODCALLTYPE setDeveloperExtrasEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE developerExtrasEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAutomaticallyDetectsCacheModel(BOOL);
    virtual HRESULT STDMETHODCALLTYPE automaticallyDetectsCacheModel(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAuthorAndUserStylesEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE authorAndUserStylesEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE inApplicationChromeMode(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setApplicationChromeMode(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setOfflineWebApplicationCacheEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE offlineWebApplicationCacheEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDatabasesEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE databasesEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLocalStorageEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE localStorageEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE localStorageDatabasePath(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setLocalStorageDatabasePath(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE experimentalNotificationsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setExperimentalNotificationsEnabled(BOOL);

    // These two methods are no-ops, and only retained to keep
    // the Interface consistent. DO NOT USE THEM.
    virtual HRESULT STDMETHODCALLTYPE setShouldPaintNativeControls(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldPaintNativeControls(_Out_ BOOL*);

    virtual HRESULT STDMETHODCALLTYPE setZoomsTextOnly(BOOL);
    virtual HRESULT STDMETHODCALLTYPE zoomsTextOnly(_Out_ BOOL *);
    virtual HRESULT STDMETHODCALLTYPE fontSmoothingContrast(_Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE setFontSmoothingContrast(float);
    virtual HRESULT STDMETHODCALLTYPE isWebSecurityEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setWebSecurityEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE allowUniversalAccessFromFileURLs(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowUniversalAccessFromFileURLs(BOOL);
    virtual HRESULT STDMETHODCALLTYPE allowFileAccessFromFileURLs(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowFileAccessFromFileURLs(BOOL);
    virtual HRESULT STDMETHODCALLTYPE javaScriptCanAccessClipboard(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptCanAccessClipboard(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isXSSAuditorEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setXSSAuditorEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setShouldUseHighResolutionTimers(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldUseHighResolutionTimers(_Out_  BOOL*);
    virtual HRESULT STDMETHODCALLTYPE isFrameFlatteningEnabled(_Out_  BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setFrameFlatteningEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setPreferenceForTest(_In_ BSTR key, _In_ BSTR value);
    virtual HRESULT STDMETHODCALLTYPE setAcceleratedCompositingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE acceleratedCompositingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCustomDragCursorsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE customDragCursorsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShowDebugBorders(BOOL);
    virtual HRESULT STDMETHODCALLTYPE showDebugBorders(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShowRepaintCounter(BOOL);
    virtual HRESULT STDMETHODCALLTYPE showRepaintCounter(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDNSPrefetchingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isDNSPrefetchingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE hyperlinkAuditingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setHyperlinkAuditingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE loadsSiteIconsIgnoringImageLoadingPreference(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLoadsSiteIconsIgnoringImageLoadingPreference(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setFullScreenEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isFullScreenEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE hixie76WebSocketProtocolEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setHixie76WebSocketProtocolEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE mediaPlaybackRequiresUserGesture(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMediaPlaybackRequiresUserGesture(BOOL);
    virtual HRESULT STDMETHODCALLTYPE mediaPlaybackAllowsInline(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMediaPlaybackAllowsInline(BOOL);
    virtual HRESULT STDMETHODCALLTYPE showsToolTipOverTruncatedText(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShowsToolTipOverTruncatedText(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shouldInvertColors(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShouldInvertColors(BOOL);
    virtual HRESULT STDMETHODCALLTYPE requestAnimationFrameEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setRequestAnimationFrameEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE mockScrollbarsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMockScrollbarsEnabled(BOOL);

    // These two methods are no-ops, and only retained to keep
    // the Interface consistent. DO NOT USE THEM.
    virtual HRESULT STDMETHODCALLTYPE screenFontSubstitutionEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setScreenFontSubstitutionEnabled(BOOL);

    virtual HRESULT STDMETHODCALLTYPE isInheritURIQueryComponentEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setEnableInheritURIQueryComponent(BOOL);

    // IWebPreferencesPrivate2
    virtual HRESULT STDMETHODCALLTYPE javaScriptRuntimeFlags(_Out_ unsigned*);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptRuntimeFlags(unsigned);
    virtual HRESULT STDMETHODCALLTYPE allowDisplayAndRunningOfInsecureContent(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowDisplayAndRunningOfInsecureContent(BOOL);

    // IWebPreferencesPrivate3
    virtual HRESULT STDMETHODCALLTYPE showTiledScrollingIndicator(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShowTiledScrollingIndicator(BOOL);
    virtual HRESULT STDMETHODCALLTYPE fetchAPIEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setFetchAPIEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE shadowDOMEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setShadowDOMEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE customElementsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCustomElementsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE modernMediaControlsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setModernMediaControlsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE webAnimationsCSSIntegrationEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setWebAnimationsCSSIntegrationEnabled(BOOL);
    
    // IWebPreferencesPrivate4
    virtual HRESULT STDMETHODCALLTYPE setApplicationId(BSTR);
    virtual HRESULT STDMETHODCALLTYPE webAnimationsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setWebAnimationsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE userTimingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setUserTimingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE resourceTimingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setResourceTimingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE linkPreloadEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLinkPreloadEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE mediaPreloadingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMediaPreloadingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE clearNetworkLoaderSession();
    virtual HRESULT STDMETHODCALLTYPE switchNetworkLoaderToNewTestingSession();

    // IWebPreferencesPrivate5
    virtual HRESULT STDMETHODCALLTYPE isSecureContextAttributeEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setIsSecureContextAttributeEnabled(BOOL);

    // IWebPreferencesPrivate6
    virtual HRESULT STDMETHODCALLTYPE dataTransferItemsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDataTransferItemsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE inspectorAdditionsEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setInspectorAdditionsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE visualViewportAPIEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setVisualViewportAPIEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE CSSOMViewScrollingAPIEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCSSOMViewScrollingAPIEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE fetchAPIKeepAliveEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setFetchAPIKeepAliveEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE spatialNavigationEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setSpatialNavigationEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE menuItemElementEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMenuItemElementEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE keygenElementEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setKeygenElementEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE serverTimingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setServerTimingEnabled(BOOL);

    // IWebPreferencesPrivate7
    virtual HRESULT STDMETHODCALLTYPE crossOriginWindowPolicySupportEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCrossOriginWindowPolicySupportEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE resizeObserverEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setResizeObserverEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE coreMathMLEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCoreMathMLEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE lazyImageLoadingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setLazyImageLoadingEnabled(BOOL);

    // WebPreferences

    // This method accesses a different preference key than developerExtrasEnabled.
    // See <rdar://5343767> for the justification.
    bool developerExtrasDisabledByOverride();

    static BSTR webPreferencesChangedNotification();
    static BSTR webPreferencesRemovedNotification();

    static void setInstance(WebPreferences* instance, BSTR identifier);
    static void removeReferenceForIdentifier(BSTR identifier);
    static WebPreferences* sharedStandardPreferences();

    static CFStringRef applicationId();

    // From WebHistory.h
    HRESULT historyItemLimit(_Out_ int*);
    HRESULT setHistoryItemLimit(int);
    HRESULT historyAgeInDaysLimit(_Out_ int*);
    HRESULT setHistoryAgeInDaysLimit(int);

    void willAddToWebView();
    void didRemoveFromWebView();

    HRESULT postPreferencesChangesNotification();

protected:
#if USE(CF)
    void setValueForKey(CFStringRef key, CFPropertyListRef value);
    RetainPtr<CFPropertyListRef> valueForKey(CFStringRef key);
    void setValueForKey(const char* key, CFPropertyListRef value);
    RetainPtr<CFPropertyListRef> valueForKey(const char* key);
#endif
    BSTR stringValueForKey(const char* key);
    int integerValueForKey(const char* key);
    BOOL boolValueForKey(const char* key);
    float floatValueForKey(const char* key);
    LONGLONG longlongValueForKey(const char* key);
    void setStringValue(const char* key, BSTR value);
    void setIntegerValue(const char* key, int value);
    void setBoolValue(const char* key, BOOL value);
    void setFloatValue(const char* key, float value);
    void setLongLongValue(const char* key, LONGLONG value);
    static WebPreferences* getInstanceForIdentifier(BSTR identifier);
    static void initializeDefaultSettings();
    void save();
    void load();
#if USE(CF)
    void migrateWebKitPreferencesToCFPreferences();
    void copyWebKitPreferencesToCFPreferences(CFDictionaryRef);
#endif

protected:
    ULONG m_refCount { 0 };
    WebCore::BString m_identifier;
    bool m_autoSaves { false };
    bool m_automaticallyDetectsCacheModel { true };
    unsigned m_numWebViews { 0 };

#if USE(CF)
    RetainPtr<CFMutableDictionaryRef> m_privatePrefs;
    static RetainPtr<CFStringRef> m_applicationId;
#endif
};
