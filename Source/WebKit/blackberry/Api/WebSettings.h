/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebSettings_h
#define WebSettings_h

#include "BlackBerryGlobal.h"

namespace BlackBerry {
namespace WebKit {

class WebSettings;
class WebSettingsPrivate;

/*!
    @struct WebSettingsDelegate
    Defines the methods that must be implemented by a delegate of WebSettings.
*/
struct BLACKBERRY_EXPORT WebSettingsDelegate {
    virtual ~WebSettingsDelegate() { }

    /*!
        Sent when the value of a setting changed as well as on instantiation of a WebSettings object.
        @param settings The WebSettings object that sent the message.
    */
    virtual void didChangeSettings(WebSettings*) = 0;
};

/*!
    @class WebSettings
*/
class BLACKBERRY_EXPORT WebSettings {
public:
    static WebSettings* createFromStandardSettings(WebSettingsDelegate* = 0);
    ~WebSettings();

    static WebSettings* standardSettings();

    void setDelegate(WebSettingsDelegate*);
    WebSettingsDelegate* delegate();

    static void addSupportedObjectPluginMIMEType(const char*);
    static bool isSupportedObjectMIMEType(const WebString&);

    bool xssAuditorEnabled() const;
    void setXSSAuditorEnabled(bool);

    bool loadsImagesAutomatically() const;
    void setLoadsImagesAutomatically(bool);

    bool shouldDrawBorderWhileLoadingImages() const;
    void setShouldDrawBorderWhileLoadingImages(bool);

    bool isJavaScriptEnabled() const;
    void setJavaScriptEnabled(bool);

    bool isPrivateBrowsingEnabled() const;
    void setPrivateBrowsingEnabled(bool);

    int defaultFixedFontSize() const;
    void setDefaultFixedFontSize(int);

    int defaultFontSize() const;
    void setDefaultFontSize(int);

    int minimumFontSize() const;
    void setMinimumFontSize(int);

    WebString serifFontFamily() const;
    void setSerifFontFamily(const char*);
    WebString fixedFontFamily() const;
    void setFixedFontFamily(const char*);
    WebString sansSerifFontFamily() const;
    void setSansSerifFontFamily(const char*);
    WebString standardFontFamily() const;
    void setStandardFontFamily(const char*);

    void setDownloadableBinaryFontsEnabled(bool);
    bool downloadableBinaryFontsEnabled() const;

    WebString userAgentString() const;
    void setUserAgentString(const WebString&);

    WebString defaultTextEncodingName() const;
    void setDefaultTextEncodingName(const char*);

    bool isZoomToFitOnLoad() const;
    void setZoomToFitOnLoad(bool);

    enum TextReflowMode { TextReflowDisabled, TextReflowEnabled, TextReflowEnabledOnlyForBlockZoom };
    TextReflowMode textReflowMode() const;
    void setTextReflowMode(TextReflowMode);

    bool isScrollbarsEnabled() const;
    void setScrollbarsEnabled(bool);

    // FIXME: Consider renaming this method upstream, where it is called javaScriptCanOpenWindowsAutomatically.
    bool canJavaScriptOpenWindowsAutomatically() const;
    void setJavaScriptOpenWindowsAutomatically(bool);

    bool arePluginsEnabled() const;
    void setPluginsEnabled(bool);

    bool isGeolocationEnabled() const;
    void setGeolocationEnabled(bool);

    // Context info
    bool doesGetFocusNodeContext() const;
    void setGetFocusNodeContext(bool);

    WebString userStyleSheetString() const;
    void setUserStyleSheetString(const char*);

    WebString userStyleSheetLocation();
    void setUserStyleSheetLocation(const char*);

    // External link handlers
    bool areLinksHandledExternally() const;
    void setAreLinksHandledExternally(bool);

    // BrowserField2 settings
    void setAllowCrossSiteRequests(bool);
    bool allowCrossSiteRequests() const;
    bool isUserScalable() const;
    void setUserScalable(bool);
    int viewportWidth() const;
    void setViewportWidth(int);
    double initialScale() const;
    void setInitialScale(double);

    int firstScheduledLayoutDelay() const;
    void setFirstScheduledLayoutDelay(int);

    // Whether to include pattern: in the list of string patterns.
    bool shouldHandlePatternUrls() const;
    void setShouldHandlePatternUrls(bool);

    bool areCookiesEnabled() const;
    void setAreCookiesEnabled(bool);

    // Web storage settings
    bool isLocalStorageEnabled() const;
    void setIsLocalStorageEnabled(bool);

    bool isDatabasesEnabled() const;
    void setIsDatabasesEnabled(bool);

    bool isAppCacheEnabled() const;
    void setIsAppCacheEnabled(bool);

    unsigned long long localStorageQuota() const;
    void setLocalStorageQuota(unsigned long long);

    // Page cache
    void setMaximumPagesInCache(int);
    int maximumPagesInCache() const;

    WebString localStoragePath() const;
    void setLocalStoragePath(const WebString&);

    WebString databasePath() const;
    void setDatabasePath(const WebString&);

    WebString appCachePath() const;
    void setAppCachePath(const WebString&);

    WebString pageGroupName() const;
    void setPageGroupName(const WebString&);

    // FIXME: We shouldn't have an email mode. Instead, we should expose all email-related settings
    // so that the email client can toggle them directly.
    bool isEmailMode() const;
    void setEmailMode(bool enable);

    bool shouldRenderAnimationsOnScrollOrZoom() const;
    void setShouldRenderAnimationsOnScrollOrZoom(bool enable);

    unsigned overZoomColor() const;
    void setOverZoomColor(unsigned);

    WebString overScrollImagePath() const;
    void setOverScrollImagePath(const char*);

    unsigned backgroundColor() const;
    void setBackgroundColor(unsigned);

    bool isWritingDirectionRTL() const;
    void setWritingDirectionRTL(bool);

    bool useWebKitCache() const;
    void setUseWebKitCache(bool);

    bool isFrameFlatteningEnabled() const;
    void setFrameFlatteningEnabled(bool);

    bool isDirectRenderingToWindowEnabled() const;
    void setDirectRenderingToWindowEnabled(bool);

    unsigned maxPluginInstances() const;
    void setMaxPluginInstances(unsigned num);

    bool areWebSocketsEnabled() const;
    void setWebSocketsEnabled(bool);

    bool isSpatialNavigationEnabled() const;
    void setSpatialNavigationEnabled(bool);

private:
    WebSettingsPrivate* m_private;
    WebSettings();
    WebSettings(const WebSettings&);
};

/*!
    @class WebSettingsTransaction
    Defines a scope guard that suppresses didChangeSettings messages within its scope.
    On destruction the guarded WebSettings object will dispatch exactly one didChangeSettings message.
*/
class BLACKBERRY_EXPORT WebSettingsTransaction {
public:
    WebSettingsTransaction(WebSettings* settings)
        : m_settings(settings)
        , m_savedDelegate(0)
    {
        if (!settings)
            return;
        m_savedDelegate = settings->delegate();
        settings->setDelegate(0);
    }

    ~WebSettingsTransaction()
    {
        if (!m_settings || !m_savedDelegate)
            return;
        m_settings->setDelegate(m_savedDelegate);
        m_savedDelegate->didChangeSettings(m_settings);
    }

private:
    WebSettingsTransaction(const WebSettingsTransaction&);
    WebSettingsTransaction& operator=(const WebSettingsTransaction&);

    WebSettings* m_settings;
    WebSettingsDelegate* m_savedDelegate;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // WebSettings_h
