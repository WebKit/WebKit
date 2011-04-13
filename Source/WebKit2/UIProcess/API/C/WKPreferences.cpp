/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKPreferences.h"
#include "WKPreferencesPrivate.h"

#include "WKAPICast.h"
#include "WebPreferences.h"
#include <WebCore/Settings.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

using namespace WebKit;

WKTypeID WKPreferencesGetTypeID()
{
    return toAPI(WebPreferences::APIType);
}

WKPreferencesRef WKPreferencesCreate()
{
    RefPtr<WebPreferences> preferences = WebPreferences::create();
    return toAPI(preferences.release().leakRef());
}

WKPreferencesRef WKPreferencesCreateWithIdentifier(WKStringRef identifierRef)
{
    RefPtr<WebPreferences> preferences = WebPreferences::create(toWTFString(identifierRef));
    return toAPI(preferences.release().releaseRef());
}

void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferencesRef, bool javaScriptEnabled)
{
    toImpl(preferencesRef)->setJavaScriptEnabled(javaScriptEnabled);
}

bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptEnabled();
}

void WKPreferencesSetLoadsImagesAutomatically(WKPreferencesRef preferencesRef, bool loadsImagesAutomatically)
{
    toImpl(preferencesRef)->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

bool WKPreferencesGetLoadsImagesAutomatically(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->loadsImagesAutomatically();
}

void WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(WKPreferencesRef preferencesRef, bool loadsSiteIconsIgnoringImageLoadingPreference)
{
    toImpl(preferencesRef)->setLoadsSiteIconsIgnoringImageLoadingPreference(loadsSiteIconsIgnoringImageLoadingPreference);
}

bool WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->loadsSiteIconsIgnoringImageLoadingPreference();
}

void WKPreferencesSetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferencesRef, bool offlineWebApplicationCacheEnabled)
{
    toImpl(preferencesRef)->setOfflineWebApplicationCacheEnabled(offlineWebApplicationCacheEnabled);
}

bool WKPreferencesGetOfflineWebApplicationCacheEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->offlineWebApplicationCacheEnabled();
}

void WKPreferencesSetLocalStorageEnabled(WKPreferencesRef preferencesRef, bool localStorageEnabled)
{
    toImpl(preferencesRef)->setLocalStorageEnabled(localStorageEnabled);
}

bool WKPreferencesGetLocalStorageEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->localStorageEnabled();
}

void WKPreferencesSetDatabasesEnabled(WKPreferencesRef preferencesRef, bool databasesEnabled)
{
    toImpl(preferencesRef)->setDatabasesEnabled(databasesEnabled);
}

bool WKPreferencesGetDatabasesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->databasesEnabled();
}

void WKPreferencesSetXSSAuditorEnabled(WKPreferencesRef preferencesRef, bool xssAuditorEnabled)
{
    toImpl(preferencesRef)->setXSSAuditorEnabled(xssAuditorEnabled);
}

bool WKPreferencesGetXSSAuditorEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->xssAuditorEnabled();
}

void WKPreferencesSetFrameFlatteningEnabled(WKPreferencesRef preferencesRef, bool frameFlatteningEnabled)
{
    toImpl(preferencesRef)->setFrameFlatteningEnabled(frameFlatteningEnabled);
}

bool WKPreferencesGetFrameFlatteningEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->frameFlatteningEnabled();
}

void WKPreferencesSetPluginsEnabled(WKPreferencesRef preferencesRef, bool pluginsEnabled)
{
    toImpl(preferencesRef)->setPluginsEnabled(pluginsEnabled);
}

bool WKPreferencesGetPluginsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->pluginsEnabled();
}

void WKPreferencesSetJavaEnabled(WKPreferencesRef preferencesRef, bool javaEnabled)
{
    toImpl(preferencesRef)->setJavaEnabled(javaEnabled);
}

bool WKPreferencesGetJavaEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaEnabled();
}

void WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef, bool javaScriptCanOpenWindowsAutomatically)
{
    toImpl(preferencesRef)->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

bool WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptCanOpenWindowsAutomatically();
}

void WKPreferencesSetHyperlinkAuditingEnabled(WKPreferencesRef preferencesRef, bool hyperlinkAuditingEnabled)
{
    toImpl(preferencesRef)->setHyperlinkAuditingEnabled(hyperlinkAuditingEnabled);
}

bool WKPreferencesGetHyperlinkAuditingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->hyperlinkAuditingEnabled();
}

void WKPreferencesSetStandardFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setStandardFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyStandardFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->standardFontFamily());
}

void WKPreferencesSetFixedFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setFixedFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFixedFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->fixedFontFamily());
}

void WKPreferencesSetSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->serifFontFamily());
}

void WKPreferencesSetSansSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setSansSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySansSerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->sansSerifFontFamily());
}

void WKPreferencesSetCursiveFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setCursiveFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyCursiveFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->cursiveFontFamily());
}

void WKPreferencesSetFantasyFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setFantasyFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFantasyFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->fantasyFontFamily());
}

void WKPreferencesSetDefaultFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toImpl(preferencesRef)->setDefaultFontSize(size);
}

uint32_t WKPreferencesGetDefaultFontSize(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->defaultFontSize();
}

void WKPreferencesSetDefaultFixedFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toImpl(preferencesRef)->setDefaultFixedFontSize(size);
}

uint32_t WKPreferencesGetDefaultFixedFontSize(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->defaultFixedFontSize();
}

void WKPreferencesSetMinimumFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toImpl(preferencesRef)->setMinimumFontSize(size);
}

uint32_t WKPreferencesGetMinimumFontSize(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->minimumFontSize();
}

void WKPreferencesSetEditableLinkBehavior(WKPreferencesRef preferencesRef, WKEditableLinkBehavior wkBehavior)
{
    toImpl(preferencesRef)->setEditableLinkBehavior(toEditableLinkBehavior(wkBehavior));
}

WKEditableLinkBehavior WKPreferencesGetEditableLinkBehavior(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(toImpl(preferencesRef)->editableLinkBehavior()));
}

void WKPreferencesSetDefaultTextEncodingName(WKPreferencesRef preferencesRef, WKStringRef name)
{
    toImpl(preferencesRef)->setDefaultTextEncodingName(toWTFString(name));
}

WKStringRef WKPreferencesCopyDefaultTextEncodingName(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->defaultTextEncodingName());
}

void WKPreferencesSetPrivateBrowsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPrivateBrowsingEnabled(enabled);
}

bool WKPreferencesGetPrivateBrowsingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->privateBrowsingEnabled();
}

void WKPreferencesSetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setDeveloperExtrasEnabled(enabled);
}

bool WKPreferencesGetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->developerExtrasEnabled();
}

void WKPreferencesSetTextAreasAreResizable(WKPreferencesRef preferencesRef, bool resizable)
{
    toImpl(preferencesRef)->setTextAreasAreResizable(resizable);
}

bool WKPreferencesGetTextAreasAreResizable(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->textAreasAreResizable();
}

void WKPreferencesSetFontSmoothingLevel(WKPreferencesRef preferencesRef, WKFontSmoothingLevel wkLevel)
{
    toImpl(preferencesRef)->setFontSmoothingLevel(toFontSmoothingLevel(wkLevel));
}

WKFontSmoothingLevel WKPreferencesGetFontSmoothingLevel(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<FontSmoothingLevel>(toImpl(preferencesRef)->fontSmoothingLevel()));
}

void WKPreferencesSetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAcceleratedDrawingEnabled(flag);
}

bool WKPreferencesGetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->acceleratedDrawingEnabled();
}

void WKPreferencesSetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCanvasUsesAcceleratedDrawing(flag);
}

bool WKPreferencesGetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->canvasUsesAcceleratedDrawing();
}

void WKPreferencesSetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAcceleratedCompositingEnabled(flag);
}

bool WKPreferencesGetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->acceleratedCompositingEnabled();
}

void WKPreferencesSetCompositingBordersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCompositingBordersVisible(flag);
}

bool WKPreferencesGetCompositingBordersVisible(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->compositingBordersVisible();
}

void WKPreferencesSetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCompositingRepaintCountersVisible(flag);
}

bool WKPreferencesGetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->compositingRepaintCountersVisible();
}

void WKPreferencesSetWebGLEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebGLEnabled(flag);
}

bool WKPreferencesGetWebGLEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webGLEnabled();
}

void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setNeedsSiteSpecificQuirks(flag);
}

bool WKPreferencesGetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->needsSiteSpecificQuirks();
}

void WKPreferencesSetForceFTPDirectoryListings(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setForceFTPDirectoryListings(flag);
}

bool WKPreferencesGetForceFTPDirectoryListings(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->forceFTPDirectoryListings();
}

void WKPreferencesSetFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef, WKStringRef pathRef)
{
    toImpl(preferencesRef)->setFTPDirectoryTemplatePath(toWTFString(pathRef));
}

WKStringRef WKPreferencesCopyFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->ftpDirectoryTemplatePath());
}

void WKPreferencesSetTabsToLinks(WKPreferencesRef preferencesRef, bool tabsToLinks)
{
    toImpl(preferencesRef)->setTabsToLinks(tabsToLinks);
}

bool WKPreferencesGetTabsToLinks(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->tabsToLinks();
}

void WKPreferencesSetDNSPrefetchingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setDNSPrefetchingEnabled(enabled);
}

bool WKPreferencesGetDNSPrefetchingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->dnsPrefetchingEnabled();
}

void WKPreferencesSetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAuthorAndUserStylesEnabled(enabled);
}

bool WKPreferencesGetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->authorAndUserStylesEnabled();
}

void WKPreferencesSetShouldPrintBackgrounds(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setShouldPrintBackgrounds(flag);
}

bool WKPreferencesGetShouldPrintBackgrounds(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldPrintBackgrounds();
}

void WKPreferencesSetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setWebArchiveDebugModeEnabled(enabled);
}

bool WKPreferencesGetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webArchiveDebugModeEnabled();
}

void WKPreferencesSetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setLocalFileContentSniffingEnabled(enabled);
}

bool WKPreferencesGetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->localFileContentSniffingEnabled();
}

void WKPreferencesSetPageCacheEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setUsesPageCache(enabled);
}

bool WKPreferencesGetPageCacheEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->usesPageCache();
}

void WKPreferencesSetPaginateDuringLayoutEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPaginateDuringLayoutEnabled(enabled);
}

bool WKPreferencesGetPaginateDuringLayoutEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->paginateDuringLayoutEnabled();
}

void WKPreferencesSetDOMPasteAllowed(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setDOMPasteAllowed(enabled);
}

bool WKPreferencesGetDOMPasteAllowed(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->domPasteAllowed();
}

void WKPreferencesSetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setJavaScriptCanAccessClipboard(enabled);
}

bool WKPreferencesGetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptCanAccessClipboard();
}

void WKPreferencesSetFullScreenEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setFullScreenEnabled(enabled);
}

bool WKPreferencesGetFullScreenEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->fullScreenEnabled();
}

void WKPreferencesSetWebSecurityEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setWebSecurityEnabled(enabled);
}

bool WKPreferencesGetWebSecurityEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webSecurityEnabled();
}

void WKPreferencesSetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    toImpl(preferencesRef)->setAllowUniversalAccessFromFileURLs(allowed);
}

bool WKPreferencesGetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowUniversalAccessFromFileURLs();
}

void WKPreferencesSetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    toImpl(preferencesRef)->setAllowFileAccessFromFileURLs(allowed);
}

bool WKPreferencesGetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowFileAccessFromFileURLs();
}
