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

#include "config.h"
#include "WebSettings.h"

#include "MIMETypeRegistry.h"
#include "WebSettings_p.h"

#include "WebString.h"
#include <Base64.h>
#include <Color.h>
#include <PageCache.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace BlackBerry {
namespace WebKit {

DEFINE_STATIC_LOCAL(String, BlackBerryAllowCrossSiteRequests, ("BlackBerryAllowCrossSiteRequests"));
DEFINE_STATIC_LOCAL(String, BlackBerryBackgroundColor, ("BlackBerryBackgroundColor"));
DEFINE_STATIC_LOCAL(String, BlackBerryCookiesEnabled, ("BlackBerryCookiesEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryDirectRenderingToWindowEnabled, ("BlackBerryDirectRenderingToWindowEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryDrawBorderWhileLoadingImages, ("BlackBerryDrawBorderWhileLoadingImages"));
DEFINE_STATIC_LOCAL(String, BlackBerryEmailModeEnabled, ("BlackBerryEmailModeEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryGetFocusNodeContextEnabled, ("BlackBerryGetFocusNodeContextEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryHandlePatternURLs, ("BlackBerryHandlePatternURLs"));
DEFINE_STATIC_LOCAL(String, BlackBerryInitialScale, ("BlackBerryInitialScale"));
DEFINE_STATIC_LOCAL(String, BlackBerryLinksHandledExternallyEnabled, ("BlackBerryLinksHandledExternallyEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryMaxPluginInstances, ("BlackBerryMaxPluginInstances"));
DEFINE_STATIC_LOCAL(String, BlackBerryOverZoomColor, ("BlackBerryOverZoomColor"));
DEFINE_STATIC_LOCAL(String, BlackBerryOverScrollImagePath, ("BlackBerryOverScrollImagePath"));
DEFINE_STATIC_LOCAL(String, BlackBerryRenderAnimationsOnScrollOrZoomEnabled, ("BlackBerryRenderAnimationsOnScrollOrZoomEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryScrollbarsEnabled, ("BlackBerryScrollbarsEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryTextReflowMode, ("BlackBerryTextReflowMode"));
DEFINE_STATIC_LOCAL(String, BlackBerryUseRTLWritingDirection, ("BlackBerryUseRTLWritingDirection"));
DEFINE_STATIC_LOCAL(String, BlackBerryUseWebKitCache, ("BlackBerryUseWebKitCache"));
DEFINE_STATIC_LOCAL(String, BlackBerryUserAgentString, ("BlackBerryUserAgentString"));
DEFINE_STATIC_LOCAL(String, BlackBerryUserScalableEnabled, ("BlackBerryUserScalableEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryViewportWidth, ("BlackBerryViewportWidth"));
DEFINE_STATIC_LOCAL(String, BlackBerryZoomToFitOnLoadEnabled, ("BlackBerryZoomToFitOnLoadEnabled"));
DEFINE_STATIC_LOCAL(String, BlackBerryFullScreenVideoCapable, ("BlackBerryFullScreenVideoCapable"));
DEFINE_STATIC_LOCAL(String, SpatialNavigationEnabled, ("SpatialNavigationEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitDatabasePath, ("WebKitDatabasePath"));
DEFINE_STATIC_LOCAL(String, WebKitDatabasesEnabled, ("WebKitDatabasesEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitDefaultFixedFontSize, ("WebKitDefaultFixedFontSize"));
DEFINE_STATIC_LOCAL(String, WebKitDefaultFontSize, ("WebKitDefaultFontSize"));
DEFINE_STATIC_LOCAL(String, WebKitDefaultTextEncodingName, ("WebKitDefaultTextEncodingName"));
DEFINE_STATIC_LOCAL(String, WebKitDownloadableBinaryFontsEnabled, ("WebKitDownloadableBinaryFontsEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitFirstScheduledLayoutDelay, ("WebKitFirstScheduledLayoutDelay"));
DEFINE_STATIC_LOCAL(String, WebKitFixedFontFamily, ("WebKitFixedFontFamily"));
DEFINE_STATIC_LOCAL(String, WebKitFrameFlatteningEnabled, ("WebKitFrameFlatteningEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitGeolocationEnabled, ("WebKitGeolocationEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitJavaScriptCanOpenWindowsAutomaticallyEnabled, ("WebKitJavaScriptCanOpenWindowsAutomaticallyEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitJavaScriptEnabled, ("WebKitJavaScriptEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitLoadsImagesAutomatically, ("WebKitLoadsImagesAutomatically"));
DEFINE_STATIC_LOCAL(String, WebKitLocalStorageEnabled, ("WebKitLocalStorageEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitLocalStoragePath, ("WebKitLocalStoragePath"));
DEFINE_STATIC_LOCAL(String, WebKitLocalStorageQuota, ("WebKitLocalStorageQuota"));
DEFINE_STATIC_LOCAL(String, WebKitMaximumPagesInCache, ("WebKitMaximumPagesInCache"));
DEFINE_STATIC_LOCAL(String, WebKitMinimumFontSize, ("WebKitMinimumFontSize"));
DEFINE_STATIC_LOCAL(String, WebKitOfflineWebApplicationCacheEnabled, ("WebKitOfflineWebApplicationCacheEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitOfflineWebApplicationCachePath, ("WebKitOfflineWebApplicationCachePath"));
DEFINE_STATIC_LOCAL(String, WebKitPageGroupName, ("WebKitPageGroupName"));
DEFINE_STATIC_LOCAL(String, WebKitPluginsEnabled, ("WebKitPluginsEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitPrivateBrowsingEnabled, ("WebKitPrivateBrowsingEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitSansSeriffFontFamily, ("WebKitSansSeriffFontFamily"));
DEFINE_STATIC_LOCAL(String, WebKitSeriffFontFamily, ("WebKitSeriffFontFamily"));
DEFINE_STATIC_LOCAL(String, WebKitStandardFontFamily, ("WebKitStandardFontFamily"));
DEFINE_STATIC_LOCAL(String, WebKitUserStyleSheet, ("WebKitUserStyleSheet"));
DEFINE_STATIC_LOCAL(String, WebKitUserStyleSheetLocation, ("WebKitUserStyleSheetLocation"));
DEFINE_STATIC_LOCAL(String, WebKitWebSocketsEnabled, ("WebKitWebSocketsEnabled"));
DEFINE_STATIC_LOCAL(String, WebKitXSSAuditorEnabled, ("WebKitXSSAuditorEnabled"));

static HashSet<String>* s_supportedObjectMIMETypes;

WebSettingsPrivate::WebSettingsPrivate()
    : impl(0)
    , delegate(0)
    , sender(0)
    , copyOnWrite(true)
{
}

WebSettings::WebSettings()
{
    m_private = new WebSettingsPrivate();
}

WebSettings::WebSettings(const WebSettings& otherSettings)
{
    m_private->impl = otherSettings.m_private->impl;
}

WebSettings::~WebSettings()
{
    if (!m_private->copyOnWrite) {
        delete m_private->impl;
        m_private->impl = 0;
    }
    delete m_private;
    m_private = 0;
}

void WebSettings::setDelegate(WebSettingsDelegate* delegate)
{
    m_private->delegate = delegate;
    m_private->sender = this;
}

WebSettingsDelegate* WebSettings::delegate()
{
    return m_private->delegate;
}

WebSettings* WebSettings::createFromStandardSettings(WebSettingsDelegate* delegate)
{
    WebSettings* settings = new WebSettings();
    settings->m_private->impl = standardSettings()->m_private->impl;
    settings->m_private->delegate = delegate;
    settings->m_private->sender = settings;
    if (delegate)
        delegate->didChangeSettings(settings);

    return settings;
}

WebSettings* WebSettings::standardSettings()
{
    static WebSettings *settings = 0;
    if (settings)
        return settings;

    settings = new WebSettings;
    settings->m_private->impl = new WebSettingsPrivate::WebSettingsPrivateImpl();
    settings->m_private->copyOnWrite = false;
    settings->m_private->setUnsigned(BlackBerryBackgroundColor, WebCore::Color::white);
    settings->m_private->setBoolean(BlackBerryCookiesEnabled, true);
    settings->m_private->setDouble(BlackBerryInitialScale, -1);
    settings->m_private->setUnsigned(BlackBerryMaxPluginInstances, 1);
    settings->m_private->setUnsigned(BlackBerryOverZoomColor, WebCore::Color::white);
    settings->m_private->setString(BlackBerryOverScrollImagePath, "");
    settings->m_private->setBoolean(BlackBerryScrollbarsEnabled, true);

    // FIXME: We should detect whether we are embedded in a browser or an email client and default to TextReflowEnabledOnlyForBlockZoom and TextReflowEnabled, respectively.
    settings->m_private->setTextReflowMode(BlackBerryTextReflowMode, TextReflowDisabled);

    settings->m_private->setBoolean(BlackBerryUseWebKitCache, true);
    settings->m_private->setBoolean(BlackBerryUserScalableEnabled, true);
    settings->m_private->setBoolean(BlackBerryZoomToFitOnLoadEnabled, true);
    settings->m_private->setBoolean(BlackBerryFullScreenVideoCapable, false);

    settings->m_private->setInteger(WebKitDefaultFontSize, 16);
    settings->m_private->setInteger(WebKitDefaultFixedFontSize, 13);
    settings->m_private->setString(WebKitDefaultTextEncodingName, "iso-8859-1");
    settings->m_private->setBoolean(WebKitDownloadableBinaryFontsEnabled, true);
    settings->m_private->setInteger(WebKitFirstScheduledLayoutDelay, 250); // Match Document::cLayoutScheduleThreshold.
    settings->m_private->setBoolean(WebKitJavaScriptEnabled, true);
    settings->m_private->setBoolean(WebKitLoadsImagesAutomatically, true);
    settings->m_private->setUnsignedLongLong(WebKitLocalStorageQuota, 5 * 1024 * 1024);
    settings->m_private->setInteger(WebKitMaximumPagesInCache, 0);
    settings->m_private->setInteger(WebKitMinimumFontSize, 8);
    settings->m_private->setBoolean(WebKitWebSocketsEnabled, true);

    return settings;
}

void WebSettings::addSupportedObjectPluginMIMEType(const char* type)
{
    if (!s_supportedObjectMIMETypes)
        s_supportedObjectMIMETypes = new HashSet<String>;

    s_supportedObjectMIMETypes->add(type);
}

bool WebSettings::isSupportedObjectMIMEType(const WebString& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    if (!s_supportedObjectMIMETypes)
        return false;

    return s_supportedObjectMIMETypes->contains(MIMETypeRegistry::getNormalizedMIMEType(mimeType));
}

bool WebSettings::xssAuditorEnabled() const
{
    return m_private->getBoolean(WebKitXSSAuditorEnabled);
}

void WebSettings::setXSSAuditorEnabled(bool enabled)
{
    return m_private->setBoolean(WebKitXSSAuditorEnabled, enabled);
}

bool WebSettings::loadsImagesAutomatically() const
{
    return m_private->getBoolean(WebKitLoadsImagesAutomatically);
}

void WebSettings::setLoadsImagesAutomatically(bool enabled)
{
    m_private->setBoolean(WebKitLoadsImagesAutomatically, enabled);
}

bool WebSettings::downloadableBinaryFontsEnabled() const
{
    return m_private->getBoolean(WebKitDownloadableBinaryFontsEnabled);
}

void WebSettings::setDownloadableBinaryFontsEnabled(bool enabled)
{
    return m_private->setBoolean(WebKitDownloadableBinaryFontsEnabled, enabled);
}

bool WebSettings::shouldDrawBorderWhileLoadingImages() const
{
    return m_private->getBoolean(BlackBerryDrawBorderWhileLoadingImages);
}

void WebSettings::setShouldDrawBorderWhileLoadingImages(bool enabled)
{
    m_private->setBoolean(BlackBerryDrawBorderWhileLoadingImages, enabled);
}

bool WebSettings::isJavaScriptEnabled() const
{
    return m_private->getBoolean(WebKitJavaScriptEnabled);
}

void WebSettings::setJavaScriptEnabled(bool enabled)
{
    m_private->setBoolean(WebKitJavaScriptEnabled, enabled);
}

bool WebSettings::isPrivateBrowsingEnabled() const
{
    return m_private->getBoolean(WebKitPrivateBrowsingEnabled);
}

void WebSettings::setPrivateBrowsingEnabled(bool enabled)
{
    m_private->setBoolean(WebKitPrivateBrowsingEnabled, enabled);
}

int WebSettings::defaultFixedFontSize() const
{
    return m_private->getInteger(WebKitDefaultFixedFontSize);
}

void WebSettings::setDefaultFixedFontSize(int defaultFixedFontSize)
{
    m_private->setInteger(WebKitDefaultFixedFontSize, defaultFixedFontSize);
}

int WebSettings::defaultFontSize() const
{
    return m_private->getInteger(WebKitDefaultFontSize);
}

void WebSettings::setDefaultFontSize(int defaultFontSize)
{
    m_private->setInteger(WebKitDefaultFontSize, defaultFontSize);
}

int WebSettings::minimumFontSize() const
{
    return m_private->getInteger(WebKitMinimumFontSize);
}

void WebSettings::setMinimumFontSize(int minimumFontSize)
{
    m_private->setInteger(WebKitMinimumFontSize, minimumFontSize);
}

WebString WebSettings::serifFontFamily() const
{
    return m_private->getString(WebKitSeriffFontFamily);
}

void WebSettings::setSerifFontFamily(const char* seriffFontFamily)
{
    m_private->setString(WebKitSeriffFontFamily, seriffFontFamily);
}

WebString WebSettings::fixedFontFamily() const
{
    return m_private->getString(WebKitFixedFontFamily);
}

void WebSettings::setFixedFontFamily(const char* fixedFontFamily)
{
    m_private->setString(WebKitFixedFontFamily, fixedFontFamily);
}

WebString WebSettings::sansSerifFontFamily() const
{
    return m_private->getString(WebKitSansSeriffFontFamily);
}

void WebSettings::setSansSerifFontFamily(const char* sansSeriffFontFamily)
{
    m_private->setString(WebKitSansSeriffFontFamily, sansSeriffFontFamily);
}

WebString WebSettings::standardFontFamily() const
{
    return m_private->getString(WebKitStandardFontFamily);
}

void WebSettings::setStandardFontFamily(const char* standardFontFamily)
{
    m_private->setString(WebKitStandardFontFamily, standardFontFamily);
}

WebString WebSettings::userAgentString() const
{
    return m_private->getString(BlackBerryUserAgentString);
}

void WebSettings::setUserAgentString(const WebString& userAgentString)
{
    m_private->setString(BlackBerryUserAgentString, userAgentString);
}

WebString WebSettings::defaultTextEncodingName() const
{
    return m_private->getString(WebKitDefaultTextEncodingName);
}

void WebSettings::setDefaultTextEncodingName(const char* defaultTextEncodingName)
{
    m_private->setString(WebKitDefaultTextEncodingName, defaultTextEncodingName);
}

bool WebSettings::isZoomToFitOnLoad() const
{
    return m_private->getBoolean(BlackBerryZoomToFitOnLoadEnabled);
}

void WebSettings::setZoomToFitOnLoad(bool enabled)
{
    m_private->setBoolean(BlackBerryZoomToFitOnLoadEnabled, enabled);
}

WebSettings::TextReflowMode WebSettings::textReflowMode() const
{
    return m_private->getTextReflowMode(BlackBerryTextReflowMode);
}

void WebSettings::setTextReflowMode(TextReflowMode textReflowMode)
{
    m_private->setTextReflowMode(BlackBerryTextReflowMode, textReflowMode);
}

bool WebSettings::isScrollbarsEnabled() const
{
    return m_private->getBoolean(BlackBerryScrollbarsEnabled);
}

void WebSettings::setScrollbarsEnabled(bool enabled)
{
    m_private->setBoolean(BlackBerryScrollbarsEnabled, enabled);
}

bool WebSettings::canJavaScriptOpenWindowsAutomatically() const
{
    return m_private->getBoolean(WebKitJavaScriptCanOpenWindowsAutomaticallyEnabled);
}

void WebSettings::setJavaScriptOpenWindowsAutomatically(bool enabled)
{
    m_private->setBoolean(WebKitJavaScriptCanOpenWindowsAutomaticallyEnabled, enabled);
}

bool WebSettings::arePluginsEnabled() const
{
    return m_private->getBoolean(WebKitPluginsEnabled);
}

void WebSettings::setPluginsEnabled(bool enabled)
{
    m_private->setBoolean(WebKitPluginsEnabled, enabled);
}

bool WebSettings::isGeolocationEnabled() const
{
    return m_private->getBoolean(WebKitGeolocationEnabled);
}

void WebSettings::setGeolocationEnabled(bool enabled)
{
    m_private->setBoolean(WebKitGeolocationEnabled, enabled);
}

bool WebSettings::doesGetFocusNodeContext() const
{
    return m_private->getBoolean(BlackBerryGetFocusNodeContextEnabled);
}

void WebSettings::setGetFocusNodeContext(bool enabled)
{
    m_private->setBoolean(BlackBerryGetFocusNodeContextEnabled, enabled);
}

WebString WebSettings::userStyleSheetString() const
{
    return m_private->getString(WebKitUserStyleSheet);
}

void WebSettings::setUserStyleSheetString(const char* userStyleSheetString)
{
    // FIXME: This doesn't seem like the appropriate place to do this as WebSettings should ideally be a state store.
    // Either the caller of this function should do this conversion or caller of the getter corresponding to this function
    // should do this conversion.

    size_t length = strlen(userStyleSheetString);
    Vector<char> data;
    data.append(userStyleSheetString, length);

    Vector<char> encodedData;
    WebCore::base64Encode(data, encodedData);

    const char prefix[] = "data:text/css;charset=utf-8;base64,";
    size_t prefixLength = sizeof(prefix) - 1;
    Vector<char> dataURL;
    dataURL.reserveCapacity(prefixLength + encodedData.size());
    dataURL.append(prefix, prefixLength);
    dataURL.append(encodedData);
    m_private->setString(WebKitUserStyleSheet, String(dataURL.data(), dataURL.size()));
}

WebString WebSettings::userStyleSheetLocation()
{
    return m_private->getString(WebKitUserStyleSheetLocation);
}

void WebSettings::setUserStyleSheetLocation(const char* userStyleSheetLocation)
{
    m_private->setString(WebKitUserStyleSheetLocation, userStyleSheetLocation);
}

bool WebSettings::areLinksHandledExternally() const
{
    return m_private->getBoolean(BlackBerryLinksHandledExternallyEnabled);
}

void WebSettings::setAreLinksHandledExternally(bool enabled)
{
    m_private->setBoolean(BlackBerryLinksHandledExternallyEnabled, enabled);
}

void WebSettings::setAllowCrossSiteRequests(bool allow)
{
    m_private->setBoolean(BlackBerryAllowCrossSiteRequests, allow);
}

bool WebSettings::allowCrossSiteRequests() const
{
    return m_private->getBoolean(BlackBerryAllowCrossSiteRequests);
}

bool WebSettings::isUserScalable() const
{
    return m_private->getBoolean(BlackBerryUserScalableEnabled);
}

void WebSettings::setUserScalable(bool enabled)
{
    m_private->setBoolean(BlackBerryUserScalableEnabled, enabled);
}

int WebSettings::viewportWidth() const
{
    return m_private->getInteger(BlackBerryViewportWidth);
}

void WebSettings::setViewportWidth(int width)
{
    m_private->setInteger(BlackBerryViewportWidth, width);
}

double WebSettings::initialScale() const
{
    return m_private->getDouble(BlackBerryInitialScale);
}

void WebSettings::setInitialScale(double initialScale)
{
    m_private->setDouble(BlackBerryInitialScale, initialScale);
}

int WebSettings::firstScheduledLayoutDelay() const
{
    return m_private->getInteger(WebKitFirstScheduledLayoutDelay);
}

void WebSettings::setFirstScheduledLayoutDelay(int delay)
{
    m_private->setInteger(WebKitFirstScheduledLayoutDelay, delay);
}

// Whether to include pattern: in the list of string patterns.
bool WebSettings::shouldHandlePatternUrls() const
{
    return m_private->getBoolean(BlackBerryHandlePatternURLs);
}

void WebSettings::setShouldHandlePatternUrls(bool handlePatternURLs)
{
    m_private->setBoolean(BlackBerryHandlePatternURLs, handlePatternURLs);
}

bool WebSettings::areCookiesEnabled() const
{
    return m_private->getBoolean(BlackBerryCookiesEnabled);
}

void WebSettings::setAreCookiesEnabled(bool enable)
{
    m_private->setBoolean(BlackBerryCookiesEnabled, enable);
}

bool WebSettings::isLocalStorageEnabled() const
{
    return m_private->getBoolean(WebKitLocalStorageEnabled);
}

void WebSettings::setIsLocalStorageEnabled(bool enable)
{
    m_private->setBoolean(WebKitLocalStorageEnabled, enable);
}

bool WebSettings::isDatabasesEnabled() const
{
    return m_private->getBoolean(WebKitDatabasesEnabled);
}

void WebSettings::setIsDatabasesEnabled(bool enable)
{
    m_private->setBoolean(WebKitDatabasesEnabled, enable);
}

bool WebSettings::isAppCacheEnabled() const
{
    return m_private->getBoolean(WebKitOfflineWebApplicationCacheEnabled);
}

void WebSettings::setIsAppCacheEnabled(bool enable)
{
    m_private->setBoolean(WebKitOfflineWebApplicationCacheEnabled, enable);
}

unsigned long long WebSettings::localStorageQuota() const
{
    return m_private->getUnsignedLongLong(WebKitLocalStorageQuota);
}

void WebSettings::setLocalStorageQuota(unsigned long long quota)
{
    m_private->setUnsignedLongLong(WebKitLocalStorageQuota, quota);
}

int WebSettings::maximumPagesInCache() const
{
    // FIXME: We shouldn't be calling into WebCore from here. This class should just be a state store.
    return WebCore::pageCache()->capacity();
}

void WebSettings::setMaximumPagesInCache(int pages)
{
    // FIXME: We shouldn't be calling into WebCore from here. This class should just be a state store.
    unsigned realPages = std::max(0, pages);
    WebCore::pageCache()->setCapacity(realPages);
    m_private->setUnsigned(WebKitMaximumPagesInCache, realPages);
}

WebString WebSettings::localStoragePath() const
{
    return m_private->getString(WebKitLocalStoragePath);
}

void WebSettings::setLocalStoragePath(const WebString& path)
{
    m_private->setString(WebKitLocalStoragePath, path);
}

WebString WebSettings::databasePath() const
{
    return m_private->getString(WebKitDatabasePath);
}

void WebSettings::setDatabasePath(const WebString& path)
{
    m_private->setString(WebKitDatabasePath, path);
}

WebString WebSettings::appCachePath() const
{
    return m_private->getString(WebKitOfflineWebApplicationCachePath);
}

void WebSettings::setAppCachePath(const WebString& path)
{
    m_private->setString(WebKitOfflineWebApplicationCachePath, path);
}

WebString WebSettings::pageGroupName() const
{
    return m_private->getString(WebKitPageGroupName);
}

void WebSettings::setPageGroupName(const WebString& pageGroupName)
{
    m_private->setString(WebKitPageGroupName, pageGroupName);
}

bool WebSettings::isEmailMode() const
{
    return m_private->getBoolean(BlackBerryEmailModeEnabled);
}

void WebSettings::setEmailMode(bool enable)
{
    m_private->setBoolean(BlackBerryEmailModeEnabled, enable);
}

bool WebSettings::shouldRenderAnimationsOnScrollOrZoom() const
{
    return m_private->getBoolean(BlackBerryRenderAnimationsOnScrollOrZoomEnabled);
}

void WebSettings::setShouldRenderAnimationsOnScrollOrZoom(bool enable)
{
    m_private->setBoolean(BlackBerryRenderAnimationsOnScrollOrZoomEnabled, enable);
}

unsigned WebSettings::overZoomColor() const
{
    return m_private->getUnsigned(BlackBerryOverZoomColor);
}

void WebSettings::setOverZoomColor(unsigned color)
{
    m_private->setUnsigned(BlackBerryOverZoomColor, color);
}

WebString WebSettings::overScrollImagePath() const
{
    return m_private->getString(BlackBerryOverScrollImagePath);
}

void WebSettings::setOverScrollImagePath(const char* path)
{
    m_private->setString(BlackBerryOverScrollImagePath, path);
}

unsigned WebSettings::backgroundColor() const
{
    return m_private->getUnsigned(BlackBerryBackgroundColor);
}

void WebSettings::setBackgroundColor(unsigned color)
{
    m_private->setUnsigned(BlackBerryBackgroundColor, color);
}

bool WebSettings::isWritingDirectionRTL() const
{
    return m_private->getBoolean(BlackBerryUseRTLWritingDirection);
}

void WebSettings::setWritingDirectionRTL(bool useRTLWritingDirection)
{
    m_private->setBoolean(BlackBerryUseRTLWritingDirection, useRTLWritingDirection);
}

bool WebSettings::useWebKitCache() const
{
    return m_private->getBoolean(BlackBerryUseWebKitCache);
}

void WebSettings::setUseWebKitCache(bool useWebKitCache)
{
    m_private->setBoolean(BlackBerryUseWebKitCache, useWebKitCache);
}

bool WebSettings::isFrameFlatteningEnabled() const
{
    return m_private->getBoolean(WebKitFrameFlatteningEnabled);
}

void WebSettings::setFrameFlatteningEnabled(bool enable)
{
    m_private->setBoolean(WebKitFrameFlatteningEnabled, enable);
}

bool WebSettings::isDirectRenderingToWindowEnabled() const
{
    return m_private->getBoolean(BlackBerryDirectRenderingToWindowEnabled);
}

void WebSettings::setDirectRenderingToWindowEnabled(bool enable)
{
    m_private->setBoolean(BlackBerryDirectRenderingToWindowEnabled, enable);
}

unsigned WebSettings::maxPluginInstances() const
{
    return m_private->getUnsigned(BlackBerryMaxPluginInstances);
}

void WebSettings::setMaxPluginInstances(unsigned maxPluginInstances)
{
    m_private->setUnsigned(BlackBerryMaxPluginInstances, maxPluginInstances);
}

bool WebSettings::areWebSocketsEnabled() const
{
    return m_private->getBoolean(WebKitWebSocketsEnabled);
}

void WebSettings::setWebSocketsEnabled(bool enable)
{
    m_private->setBoolean(WebKitWebSocketsEnabled, enable);
}

bool WebSettings::isSpatialNavigationEnabled() const
{
    return m_private->getBoolean(SpatialNavigationEnabled);
}

void WebSettings::setSpatialNavigationEnabled(bool enable)
{
    m_private->setBoolean(SpatialNavigationEnabled, enable);
}

bool WebSettings::fullScreenVideoCapable() const
{
    return m_private->getBoolean(BlackBerryFullScreenVideoCapable);
}

void WebSettings::setFullScreenVideoCapable(bool enable)
{
    m_private->setBoolean(BlackBerryFullScreenVideoCapable, enable);
}

} // namespace WebKit
} // namespace BlackBerry
