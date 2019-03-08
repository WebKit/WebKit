/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "PluginProcessManager.h"
#include "WKPreferencesRef.h"
#include "WKPreferencesRefPrivate.h"
#include "WKAPICast.h"
#include "WebPreferences.h"
#include <WebCore/Settings.h>
#include <wtf/RefPtr.h>

using namespace WebKit;

WKTypeID WKPreferencesGetTypeID()
{
    return toAPI(WebPreferences::APIType);
}

WKPreferencesRef WKPreferencesCreate()
{
    auto preferences = WebPreferences::createWithLegacyDefaults(String(), "WebKit2.", "WebKit2.");
    return toAPI(&preferences.leakRef());
}

WKPreferencesRef WKPreferencesCreateWithIdentifier(WKStringRef identifierRef)
{
    auto preferences = WebPreferences::createWithLegacyDefaults(toWTFString(identifierRef), "WebKit2.", "WebKit2.");
    return toAPI(&preferences.leakRef());
}

WKPreferencesRef WKPreferencesCreateCopy(WKPreferencesRef preferencesRef)
{
    auto preferences = toImpl(preferencesRef)->copy();
    return toAPI(&preferences.leakRef());
}

void WKPreferencesEnableAllExperimentalFeatures(WKPreferencesRef preferencesRef)
{
    toImpl(preferencesRef)->enableAllExperimentalFeatures();
}

void WKPreferencesSetExperimentalFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef experimentalFeatureKey)
{
    toImpl(preferencesRef)->setExperimentalFeatureEnabledForKey(toWTFString(experimentalFeatureKey), value);
}

void WKPreferencesResetAllInternalDebugFeatures(WKPreferencesRef preferencesRef)
{
    toImpl(preferencesRef)->resetAllInternalDebugFeatures();
}

void WKPreferencesSetInternalDebugFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef internalDebugFeatureKey)
{
    toImpl(preferencesRef)->setInternalDebugFeatureEnabledForKey(toWTFString(internalDebugFeatureKey), value);
}

void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferencesRef, bool javaScriptEnabled)
{
    toImpl(preferencesRef)->setJavaScriptEnabled(javaScriptEnabled);
}

bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptEnabled();
}

void WKPreferencesSetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef, bool javaScriptMarkupEnabled)
{
    toImpl(preferencesRef)->setJavaScriptMarkupEnabled(javaScriptMarkupEnabled);
}

bool WKPreferencesGetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptMarkupEnabled();
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

void WKPreferencesSetJavaEnabledForLocalFiles(WKPreferencesRef preferencesRef, bool javaEnabledForLocalFiles)
{
    toImpl(preferencesRef)->setJavaEnabledForLocalFiles(javaEnabledForLocalFiles);
}

bool WKPreferencesGetJavaEnabledForLocalFiles(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaEnabledForLocalFiles();
}

void WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef, bool javaScriptCanOpenWindowsAutomatically)
{
    toImpl(preferencesRef)->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

bool WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptCanOpenWindowsAutomatically();
}

void WKPreferencesSetStorageAccessPromptsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setStorageAccessPromptsEnabled(enabled);
}

bool WKPreferencesGetStorageAccessPromptsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->storageAccessPromptsEnabled();
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

void WKPreferencesSetPictographFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toImpl(preferencesRef)->setPictographFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyPictographFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->pictographFontFamily());
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


void WKPreferencesSetCookieEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setCookieEnabled(enabled);
}

bool WKPreferencesGetCookieEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->cookieEnabled();
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

void WKPreferencesSetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef, WKJavaScriptRuntimeFlagSet javaScriptRuntimeFlagSet)
{
    toImpl(preferencesRef)->setJavaScriptRuntimeFlags(javaScriptRuntimeFlagSet);
}

WKJavaScriptRuntimeFlagSet WKPreferencesGetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->javaScriptRuntimeFlags();
}

void WKPreferencesSetTextAreasAreResizable(WKPreferencesRef preferencesRef, bool resizable)
{
    toImpl(preferencesRef)->setTextAreasAreResizable(resizable);
}

bool WKPreferencesGetTextAreasAreResizable(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->textAreasAreResizable();
}

void WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setSubpixelAntialiasedLayerTextEnabled(flag);
}

bool WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->subpixelAntialiasedLayerTextEnabled();
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

void WKPreferencesSetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setTiledScrollingIndicatorVisible(flag);
}

bool WKPreferencesGetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->tiledScrollingIndicatorVisible();
}

void WKPreferencesSetWebGLEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebGLEnabled(flag);
}

bool WKPreferencesGetWebGLEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webGLEnabled();
}

void WKPreferencesSetForceSoftwareWebGLRendering(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setForceSoftwareWebGLRendering(flag);
}

bool WKPreferencesGetForceSoftwareWebGLRendering(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->forceSoftwareWebGLRendering();
}

void WKPreferencesSetAccelerated2DCanvasEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAccelerated2dCanvasEnabled(flag);
}

bool WKPreferencesGetAccelerated2DCanvasEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->accelerated2dCanvasEnabled();
}

void WKPreferencesSetWebAnimationsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebAnimationsEnabled(flag);
}

bool WKPreferencesGetWebAnimationsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webAnimationsEnabled();
}

void WKPreferencesSetWebAnimationsCSSIntegrationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebAnimationsCSSIntegrationEnabled(flag);
}

bool WKPreferencesGetWebAnimationsCSSIntegrationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webAnimationsCSSIntegrationEnabled();
}

void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setNeedsSiteSpecificQuirks(flag);
}

bool WKPreferencesUseLegacyTextAlignPositionedElementBehavior(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->useLegacyTextAlignPositionedElementBehavior();
}

void WKPreferencesSetUseLegacyTextAlignPositionedElementBehavior(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setUseLegacyTextAlignPositionedElementBehavior(flag);
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

void WKPreferencesSetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setDOMTimersThrottlingEnabled(enabled);
}

bool WKPreferencesGetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->domTimersThrottlingEnabled();
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

void WKPreferencesSetPageCacheSupportsPlugins(WKPreferencesRef preferencesRef, bool pageCacheSupportsPlugins)
{
    toImpl(preferencesRef)->setPageCacheSupportsPlugins(pageCacheSupportsPlugins);
}

bool WKPreferencesGetPageCacheSupportsPlugins(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->pageCacheSupportsPlugins();
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

void WKPreferencesSetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAsynchronousSpellCheckingEnabled(enabled);
}

bool WKPreferencesGetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->asynchronousSpellCheckingEnabled();
}

void WKPreferencesSetAVFoundationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAVFoundationEnabled(enabled);
}

bool WKPreferencesGetAVFoundationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->isAVFoundationEnabled();
}

void WKPreferencesSetAVFoundationNSURLSessionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAVFoundationNSURLSessionEnabled(enabled);
}

bool WKPreferencesGetAVFoundationNSURLSessionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->isAVFoundationNSURLSessionEnabled();
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

void WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef, bool needsQuirk)
{
    toImpl(preferencesRef)->setNeedsStorageAccessFromFileURLsQuirk(needsQuirk);
}

bool WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->needsStorageAccessFromFileURLsQuirk();
}

void WKPreferencesSetHixie76WebSocketProtocolEnabled(WKPreferencesRef, bool /*enabled*/)
{
}

bool WKPreferencesGetHixie76WebSocketProtocolEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setRequiresUserGestureForMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->requiresUserGestureForMediaPlayback();
}

void WKPreferencesSetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setRequiresUserGestureForVideoPlayback(flag);
}

bool WKPreferencesGetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->requiresUserGestureForVideoPlayback();
}

void WKPreferencesSetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setRequiresUserGestureForAudioPlayback(flag);
}

bool WKPreferencesGetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->requiresUserGestureForAudioPlayback();
}

void WKPreferencesSetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setMainContentUserGestureOverrideEnabled(flag);
}

bool WKPreferencesGetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mainContentUserGestureOverrideEnabled();
}

void WKPreferencesSetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAllowsInlineMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowsInlineMediaPlayback();
}

void WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setInlineMediaPlaybackRequiresPlaysInlineAttribute(flag);
}

bool WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

void WKPreferencesSetBeaconAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setBeaconAPIEnabled(flag);
}

bool WKPreferencesGetBeaconAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->beaconAPIEnabled();
}

void WKPreferencesSetDirectoryUploadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setDirectoryUploadEnabled(flag);
}

bool WKPreferencesGetDirectoryUploadEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->directoryUploadEnabled();
}

void WKPreferencesSetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setMediaControlsScaleWithPageZoom(flag);
}

bool WKPreferencesGetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaControlsScaleWithPageZoom();
}

void WKPreferencesSetModernMediaControlsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setModernMediaControlsEnabled(flag);
}

bool WKPreferencesGetModernMediaControlsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->modernMediaControlsEnabled();
}

void WKPreferencesSetWebAuthenticationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebAuthenticationEnabled(flag);
}

bool WKPreferencesGetWebAuthenticationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webAuthenticationEnabled();
}

void WKPreferencesSetWebAuthenticationLocalAuthenticatorEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebAuthenticationLocalAuthenticatorEnabled(flag);
}

bool WKPreferencesGetWebAuthenticationLocalAuthenticatorEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webAuthenticationLocalAuthenticatorEnabled();
}

void WKPreferencesSetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setInvisibleAutoplayNotPermitted(!flag);
}

bool WKPreferencesGetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef)
{
    return !toImpl(preferencesRef)->invisibleAutoplayNotPermitted();
}

void WKPreferencesSetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setShowsToolTipOverTruncatedText(flag);
}

bool WKPreferencesGetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->showsToolTipOverTruncatedText();
}

void WKPreferencesSetMockScrollbarsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setMockScrollbarsEnabled(flag);
}

bool WKPreferencesGetMockScrollbarsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mockScrollbarsEnabled();
}

void WKPreferencesSetAttachmentElementEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAttachmentElementEnabled(flag);
}

bool WKPreferencesGetAttachmentElementEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->attachmentElementEnabled();
}

void WKPreferencesSetWebAudioEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setWebAudioEnabled(enabled);
}

bool WKPreferencesGetWebAudioEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webAudioEnabled();
}

void WKPreferencesSetApplicationChromeModeEnabled(WKPreferencesRef, bool)
{
    // FIXME: Remove once WebKit nightlies don't need to support Safari 8.
}

bool WKPreferencesGetApplicationChromeModeEnabled(WKPreferencesRef)
{
    // FIXME: Remove once WebKit nightlies don't need to support Safari 8.
    return false;
}

void WKPreferencesSetInspectorUsesWebKitUserInterface(WKPreferencesRef, bool)
{
    // FIXME: Remove once WebKit nightlies don't need to support Safari 6 thru 7.
}

bool WKPreferencesGetInspectorUsesWebKitUserInterface(WKPreferencesRef)
{
    // FIXME: Remove once WebKit nightlies don't need to support Safari 6 thru 7.
    return false;
}

void WKPreferencesSetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSuppressesIncrementalRendering(enabled);
}

bool WKPreferencesGetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->suppressesIncrementalRendering();
}

void WKPreferencesSetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setBackspaceKeyNavigationEnabled(enabled);
}

bool WKPreferencesGetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->backspaceKeyNavigationEnabled();
}

void WKPreferencesSetCaretBrowsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setCaretBrowsingEnabled(enabled);
}

bool WKPreferencesGetCaretBrowsingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->caretBrowsingEnabled();
}

void WKPreferencesSetShouldDisplaySubtitles(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setShouldDisplaySubtitles(enabled);
}

bool WKPreferencesGetShouldDisplaySubtitles(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldDisplaySubtitles();
}

void WKPreferencesSetShouldDisplayCaptions(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setShouldDisplayCaptions(enabled);
}

bool WKPreferencesGetShouldDisplayCaptions(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldDisplayCaptions();
}

void WKPreferencesSetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setShouldDisplayTextDescriptions(enabled);
}

bool WKPreferencesGetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldDisplayTextDescriptions();
}

void WKPreferencesSetNotificationsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setNotificationsEnabled(enabled);
}

bool WKPreferencesGetNotificationsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->notificationsEnabled();
}

void WKPreferencesSetShouldRespectImageOrientation(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setShouldRespectImageOrientation(enabled);
}

bool WKPreferencesGetShouldRespectImageOrientation(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldRespectImageOrientation();
}

void WKPreferencesSetRequestAnimationFrameEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setRequestAnimationFrameEnabled(flag);
}

bool WKPreferencesGetRequestAnimationFrameEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->requestAnimationFrameEnabled();
}

void WKPreferencesSetStorageBlockingPolicy(WKPreferencesRef preferencesRef, WKStorageBlockingPolicy policy)
{
    toImpl(preferencesRef)->setStorageBlockingPolicy(toStorageBlockingPolicy(policy));
}

WKStorageBlockingPolicy WKPreferencesGetStorageBlockingPolicy(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::SecurityOrigin::StorageBlockingPolicy>(toImpl(preferencesRef)->storageBlockingPolicy()));
}

void WKPreferencesResetTestRunnerOverrides(WKPreferencesRef preferencesRef)
{
    // Currently we reset the overrides on the web process when preferencesDidChange() is called. Since WTR preferences
    // are usually always the same (in the UI process), they are not sent to web process, not triggering the reset.
    toImpl(preferencesRef)->forceUpdate();
}

void WKPreferencesSetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setDiagnosticLoggingEnabled(enabled);
}

bool WKPreferencesGetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->diagnosticLoggingEnabled();
}

void WKPreferencesSetAsynchronousPluginInitializationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAsynchronousPluginInitializationEnabled(enabled);
}

bool WKPreferencesGetAsynchronousPluginInitializationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->asynchronousPluginInitializationEnabled();
}

void WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAsynchronousPluginInitializationEnabledForAllPlugins(enabled);
}

bool WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->asynchronousPluginInitializationEnabledForAllPlugins();
}

void WKPreferencesSetArtificialPluginInitializationDelayEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setArtificialPluginInitializationDelayEnabled(enabled);
}

bool WKPreferencesGetArtificialPluginInitializationDelayEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->artificialPluginInitializationDelayEnabled();
}

void WKPreferencesSetTabToLinksEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setTabToLinksEnabled(enabled);
}

bool WKPreferencesGetTabToLinksEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->tabToLinksEnabled();
}

void WKPreferencesSetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setInteractiveFormValidationEnabled(enabled);
}

bool WKPreferencesGetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->interactiveFormValidationEnabled();
}

void WKPreferencesSetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setScrollingPerformanceLoggingEnabled(enabled);
}

bool WKPreferencesGetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->scrollingPerformanceLoggingEnabled();
}

void WKPreferencesSetPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPlugInSnapshottingEnabled(enabled);
}

bool WKPreferencesGetPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->plugInSnapshottingEnabled();
}

void WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef preferencesRef, bool enabled)
{
#if ENABLE(NETSCAPE_PLUGIN_API) && PLATFORM(MAC)
    WebKit::PluginProcessManager::singleton().setExperimentalPlugInSandboxProfilesEnabled(enabled);
#endif
    toImpl(preferencesRef)->setExperimentalPlugInSandboxProfilesEnabled(enabled);
}

bool WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->experimentalPlugInSandboxProfilesEnabled();
}

void WKPreferencesSetSnapshotAllPlugIns(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSnapshotAllPlugIns(enabled);
}

bool WKPreferencesGetSnapshotAllPlugIns(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->snapshotAllPlugIns();
}

void WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAutostartOriginPlugInSnapshottingEnabled(enabled);
}

bool WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->autostartOriginPlugInSnapshottingEnabled();
}

void WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPrimaryPlugInSnapshotDetectionEnabled(enabled);
}

bool WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->primaryPlugInSnapshotDetectionEnabled();
}

void WKPreferencesSetPDFPluginEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPDFPluginEnabled(enabled);
}

bool WKPreferencesGetPDFPluginEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->pdfPluginEnabled();
}

void WKPreferencesSetEncodingDetectorEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setUsesEncodingDetector(enabled);
}

bool WKPreferencesGetEncodingDetectorEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->usesEncodingDetector();
}

void WKPreferencesSetTextAutosizingEnabled(WKPreferencesRef preferencesRef, bool textAutosizingEnabled)
{
    toImpl(preferencesRef)->setTextAutosizingEnabled(textAutosizingEnabled);
}

bool WKPreferencesGetTextAutosizingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->textAutosizingEnabled();
}

void WKPreferencesSetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAggressiveTileRetentionEnabled(enabled);
}

bool WKPreferencesGetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->aggressiveTileRetentionEnabled();
}

void WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setLogsPageMessagesToSystemConsoleEnabled(enabled);
}

bool WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->logsPageMessagesToSystemConsoleEnabled();
}

void WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

bool WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->pageVisibilityBasedProcessSuppressionEnabled();
}

void WKPreferencesSetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSmartInsertDeleteEnabled(enabled);
}

bool WKPreferencesGetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->smartInsertDeleteEnabled();
}

void WKPreferencesSetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSelectTrailingWhitespaceEnabled(enabled);
}

bool WKPreferencesGetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->selectTrailingWhitespaceEnabled();
}

void WKPreferencesSetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setShowsURLsInToolTipsEnabled(enabled);
}

bool WKPreferencesGetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->showsURLsInToolTipsEnabled();
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setHiddenPageDOMTimerThrottlingEnabled(enabled);
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setHiddenPageDOMTimerThrottlingAutoIncreases(enabled);
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->hiddenPageDOMTimerThrottlingEnabled();
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->hiddenPageDOMTimerThrottlingAutoIncreases();
}

void WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setHiddenPageCSSAnimationSuspensionEnabled(enabled);
}

bool WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->hiddenPageCSSAnimationSuspensionEnabled();
}

void WKPreferencesSetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef, double timeout)
{
    toImpl(preferencesRef)->setIncrementalRenderingSuppressionTimeout(timeout);
}

double WKPreferencesGetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef)
{
    return toAPI(toImpl(preferencesRef)->incrementalRenderingSuppressionTimeout());
}

void WKPreferencesSetThreadedScrollingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setThreadedScrollingEnabled(enabled);
}

bool WKPreferencesGetThreadedScrollingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->threadedScrollingEnabled();
}

void WKPreferencesSetSimpleLineLayoutEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setSimpleLineLayoutEnabled(flag);
}

bool WKPreferencesGetSimpleLineLayoutEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->simpleLineLayoutEnabled();
}

void WKPreferencesSetSimpleLineLayoutDebugBordersEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setSimpleLineLayoutDebugBordersEnabled(flag);
}

bool WKPreferencesGetSimpleLineLayoutDebugBordersEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->simpleLineLayoutDebugBordersEnabled();
}

void WKPreferencesSetContentChangeObserverEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setContentChangeObserverEnabled(flag);
}

bool WKPreferencesGetContentChangeObserverEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->contentChangeObserverEnabled();
}

void WKPreferencesSetNewBlockInsideInlineModelEnabled(WKPreferencesRef, bool)
{
    // FIXME: Remove Safari call to this.
}

bool WKPreferencesGetNewBlockInsideInlineModelEnabled(WKPreferencesRef)
{
    // FIXME: Remove Safari call for this.
    return false;
}

void WKPreferencesSetDeferredCSSParserEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setDeferredCSSParserEnabled(flag);
}

bool WKPreferencesGetDeferredCSSParserEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->deferredCSSParserEnabled();
}

void WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setSubpixelCSSOMElementMetricsEnabled(flag);
}

bool WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->subpixelCSSOMElementMetricsEnabled();
}

void WKPreferencesSetUseGiantTiles(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setUseGiantTiles(flag);
}

bool WKPreferencesGetUseGiantTiles(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->useGiantTiles();
}

void WKPreferencesSetMediaDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMediaDevicesEnabled(enabled);
}

bool WKPreferencesGetMediaDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaDevicesEnabled();
}

void WKPreferencesSetMediaStreamEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMediaStreamEnabled(enabled);
}

bool WKPreferencesGetMediaStreamEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaStreamEnabled();
}

void WKPreferencesSetPeerConnectionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setPeerConnectionEnabled(enabled);
}

bool WKPreferencesGetPeerConnectionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->peerConnectionEnabled();
}

void WKPreferencesSetWebRTCLegacyAPIEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetWebRTCLegacyAPIEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetWebRTCMDNSICECandidatesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setWebRTCMDNSICECandidatesEnabled(enabled);
}

bool WKPreferencesGetWebRTCMDNSICECandidatesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webRTCMDNSICECandidatesEnabled();
}

void WKPreferencesSetSpatialNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSpatialNavigationEnabled(enabled);
}

bool WKPreferencesGetSpatialNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->spatialNavigationEnabled();
}

void WKPreferencesSetMediaSourceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMediaSourceEnabled(enabled);
}

bool WKPreferencesGetMediaSourceEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaSourceEnabled();
}

void WKPreferencesSetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setSourceBufferChangeTypeEnabled(enabled);
}

bool WKPreferencesGetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->sourceBufferChangeTypeEnabled();
}

void WKPreferencesSetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setViewGestureDebuggingEnabled(enabled);
}

bool WKPreferencesGetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->viewGestureDebuggingEnabled();
}

void WKPreferencesSetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef, bool convert)
{
    toImpl(preferencesRef)->setShouldConvertPositionStyleOnCopy(convert);
}

bool WKPreferencesGetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldConvertPositionStyleOnCopy();
}

void WKPreferencesSetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setTelephoneNumberParsingEnabled(enabled);
}

bool WKPreferencesGetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->telephoneNumberParsingEnabled();
}

void WKPreferencesSetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setEnableInheritURIQueryComponent(enabled);
}

bool WKPreferencesGetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->enableInheritURIQueryComponent();
}

void WKPreferencesSetServiceControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setServiceControlsEnabled(enabled);
}

bool WKPreferencesGetServiceControlsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->serviceControlsEnabled();
}

void WKPreferencesSetImageControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setImageControlsEnabled(enabled);
}

bool WKPreferencesGetImageControlsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->imageControlsEnabled();
}

void WKPreferencesSetGamepadsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setGamepadsEnabled(enabled);
}

bool WKPreferencesGetGamepadsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->gamepadsEnabled();
}

// FIXME: Remove these when possible.
void WKPreferencesSetLongMousePressEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
}

bool WKPreferencesGetLongMousePressEnabled(WKPreferencesRef preferencesRef)
{
    return false;
}

void WKPreferencesSetMinimumZoomFontSize(WKPreferencesRef preferencesRef, double size)
{
    toImpl(preferencesRef)->setMinimumZoomFontSize(size);
}

double WKPreferencesGetMinimumZoomFontSize(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->minimumZoomFontSize();
}

void WKPreferencesSetAntialiasedFontDilationEnabled(WKPreferencesRef, bool)
{
    // Feature removed.
}

bool WKPreferencesGetAntialiasedFontDilationEnabled(WKPreferencesRef)
{
    return false; // Feature removed.
}

void WKPreferencesSetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef, WKDebugOverlayRegions visibleRegions)
{
    toImpl(preferencesRef)->setVisibleDebugOverlayRegions(visibleRegions);
}

WKDebugOverlayRegions WKPreferencesGetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->visibleDebugOverlayRegions();
}

void WKPreferencesSetIgnoreViewportScalingConstraints(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setIgnoreViewportScalingConstraints(enabled);
}

bool WKPreferencesGetIgnoreViewportScalingConstraints(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->ignoreViewportScalingConstraints();
}

void WKPreferencesSetMetaRefreshEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetMetaRefreshEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->httpEquivEnabled();
}

void WKPreferencesSetHTTPEquivEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetHTTPEquivEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->httpEquivEnabled();
}

void WKPreferencesSetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAllowsAirPlayForMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowsAirPlayForMediaPlayback();
}

void WKPreferencesSetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef, _WKUserInterfaceDirectionPolicy userInterfaceDirectionPolicy)
{
    toImpl(preferencesRef)->setUserInterfaceDirectionPolicy(userInterfaceDirectionPolicy);
}

_WKUserInterfaceDirectionPolicy WKPreferencesGetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef)
{
    return static_cast<_WKUserInterfaceDirectionPolicy>(toImpl(preferencesRef)->userInterfaceDirectionPolicy());
}

void WKPreferencesSetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setResourceUsageOverlayVisible(enabled);
}

bool WKPreferencesGetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->resourceUsageOverlayVisible();
}

void WKPreferencesSetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMockCaptureDevicesEnabled(enabled);
}

bool WKPreferencesGetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mockCaptureDevicesEnabled();
}

void WKPreferencesSetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setICECandidateFilteringEnabled(enabled);
}

bool WKPreferencesGetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->iceCandidateFilteringEnabled();
}

void WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

bool WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->enumeratingAllNetworkInterfacesEnabled();
}

void WKPreferencesSetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMediaCaptureRequiresSecureConnection(enabled);
}

bool WKPreferencesGetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaCaptureRequiresSecureConnection();
}

void WKPreferencesSetInactiveMediaCaptureSteamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef, double interval)
{
    toImpl(preferencesRef)->setInactiveMediaCaptureSteamRepromptIntervalInMinutes(interval);
}

double WKPreferencesGetInactiveMediaCaptureSteamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->inactiveMediaCaptureSteamRepromptIntervalInMinutes();
}

void WKPreferencesSetFetchAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setFetchAPIEnabled(flag);
}

bool WKPreferencesGetFetchAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->fetchAPIEnabled();
}

void WKPreferencesSetDisplayContentsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setDisplayContentsEnabled(flag);
}

bool WKPreferencesGetDisplayContentsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->displayContentsEnabled();
}

void WKPreferencesSetDataTransferItemsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setDataTransferItemsEnabled(flag);
}

bool WKPreferencesGetDataTransferItemsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->dataTransferItemsEnabled();
}

void WKPreferencesSetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCustomPasteboardDataEnabled(flag);
}

bool WKPreferencesGetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->customPasteboardDataEnabled();
}

void WKPreferencesSetWebShareEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebShareEnabled(flag);
}

bool WKPreferencesGetWebShareEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webShareEnabled();
}

void WKPreferencesSetDownloadAttributeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setDownloadAttributeEnabled(flag);
}

bool WKPreferencesGetDownloadAttributeEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->downloadAttributeEnabled();
}

void WKPreferencesSetIntersectionObserverEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setIntersectionObserverEnabled(flag);
}

bool WKPreferencesGetIntersectionObserverEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->intersectionObserverEnabled();
}

void WKPreferencesSetMenuItemElementEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    return toImpl(preferencesRef)->setMenuItemElementEnabled(flag);
}

bool WKPreferencesGetMenuItemElementEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->menuItemElementEnabled();
}

void WKPreferencesSetUserTimingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setUserTimingEnabled(flag);
}

bool WKPreferencesGetUserTimingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->userTimingEnabled();
}

void WKPreferencesSetResourceTimingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setResourceTimingEnabled(flag);
}

bool WKPreferencesGetResourceTimingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->resourceTimingEnabled();
}

void WKPreferencesSetFetchAPIKeepAliveEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setFetchAPIKeepAliveEnabled(flag);
}

bool WKPreferencesGetFetchAPIKeepAliveEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->fetchAPIKeepAliveEnabled();
}

void WKPreferencesSetSelectionPaintingWithoutSelectionGapsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setSelectionPaintingWithoutSelectionGapsEnabled(flag);
}

bool WKPreferencesGetSelectionPaintingWithoutSelectionGapsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->selectionPaintingWithoutSelectionGapsEnabled();
}

void WKPreferencesSetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setAllowsPictureInPictureMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowsPictureInPictureMediaPlayback();
}

WK_EXPORT bool WKPreferencesGetApplePayEnabled(WKPreferencesRef preferencesRef)
{
    return WebKit::toImpl(preferencesRef)->applePayEnabled();
}

void WKPreferencesSetApplePayEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    WebKit::toImpl(preferencesRef)->setApplePayEnabled(enabled);
}

bool WKPreferencesGetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef)
{
    return WebKit::toImpl(preferencesRef)->applePayCapabilityDisclosureAllowed();
}

void WKPreferencesSetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    WebKit::toImpl(preferencesRef)->setApplePayCapabilityDisclosureAllowed(allowed);
}

void WKPreferencesSetLinkPreloadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setLinkPreloadEnabled(flag);
}

bool WKPreferencesGetLinkPreloadEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->linkPreloadEnabled();
}

void WKPreferencesSetMediaPreloadingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setMediaPreloadingEnabled(flag);
}

bool WKPreferencesGetMediaPreloadingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaPreloadingEnabled();
}

void WKPreferencesSetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setLargeImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->largeImageAsyncDecodingEnabled();
}

void WKPreferencesSetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAnimatedImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->animatedImageAsyncDecodingEnabled();
}

void WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(flag);
}

bool WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

void WKPreferencesSetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setMediaUserGestureInheritsFromDocument(flag);
}

bool WKPreferencesGetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaUserGestureInheritsFromDocument();
}

void WKPreferencesSetMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef, WKStringRef codecs)
{
    toImpl(preferencesRef)->setMediaContentTypesRequiringHardwareSupport(toWTFString(codecs));
}

WKStringRef WKPreferencesCopyMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toImpl(preferencesRef)->mediaContentTypesRequiringHardwareSupport());
}

void WKPreferencesSetIsSecureContextAttributeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setIsSecureContextAttributeEnabled(flag);
}

bool WKPreferencesGetIsSecureContextAttributeEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->isSecureContextAttributeEnabled();
}

bool WKPreferencesGetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->legacyEncryptedMediaAPIEnabled();
}

void WKPreferencesSetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    return toImpl(preferencesRef)->setLegacyEncryptedMediaAPIEnabled(enabled);
}

bool WKPreferencesGetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowMediaContentTypesRequiringHardwareSupportAsFallback();
}

void WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef, bool allow)
{
    return toImpl(preferencesRef)->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(allow);
}

void WKPreferencesSetInspectorAdditionsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setInspectorAdditionsEnabled(flag);
}

bool WKPreferencesGetInspectorAdditionsEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->inspectorAdditionsEnabled();
}

void WKPreferencesSetStorageAccessAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setStorageAccessAPIEnabled(flag);
}

bool WKPreferencesGetStorageAccessAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->storageAccessAPIEnabled();
}

void WKPreferencesSetAccessibilityObjectModelEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAccessibilityObjectModelEnabled(flag);
}

bool WKPreferencesGetAccessibilityObjectModelEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->accessibilityObjectModelEnabled();
}

void WKPreferencesSetAriaReflectionEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAriaReflectionEnabled(flag);
}

bool WKPreferencesGetAriaReflectionEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->ariaReflectionEnabled();
}

void WKPreferencesSetCSSOMViewScrollingAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCSSOMViewScrollingAPIEnabled(flag);
}

bool WKPreferencesGetCSSOMViewScrollingAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->cssOMViewScrollingAPIEnabled();
}

void WKPreferencesSetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setShouldAllowUserInstalledFonts(flag);
}

bool WKPreferencesGetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->shouldAllowUserInstalledFonts();
}

void WKPreferencesSetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toImpl(preferencesRef)->setMediaCapabilitiesEnabled(enabled);
}

bool WKPreferencesGetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->mediaCapabilitiesEnabled();
}

void WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setAllowCrossOriginSubresourcesToAskForCredentials(flag);
}

bool WKPreferencesGetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->allowCrossOriginSubresourcesToAskForCredentials();
}

void WKPreferencesSetCrossOriginResourcePolicyEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCrossOriginResourcePolicyEnabled(flag);
}

bool WKPreferencesGetCrossOriginResourcePolicyEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->crossOriginResourcePolicyEnabled();
}

void WKPreferencesSetRestrictedHTTPResponseAccess(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setRestrictedHTTPResponseAccess(flag);
}

bool WKPreferencesGetRestrictedHTTPResponseAccess(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->restrictedHTTPResponseAccess();
}

void WKPreferencesSetServerTimingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setServerTimingEnabled(flag);
}

bool WKPreferencesGetServerTimingEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->serverTimingEnabled();
}

void WKPreferencesSetColorFilterEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setColorFilterEnabled(flag);
}

bool WKPreferencesGetColorFilterEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->colorFilterEnabled();
}

void WKPreferencesSetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setProcessSwapOnCrossSiteNavigationEnabled(flag);
}

bool WKPreferencesGetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->processSwapOnCrossSiteNavigationEnabled();
}

void WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setPunchOutWhiteBackgroundsInDarkMode(flag);
}

bool WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->punchOutWhiteBackgroundsInDarkMode();
}

void WKPreferencesSetWebSQLDisabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setWebSQLDisabled(flag);
}

bool WKPreferencesGetWebSQLDisabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->webSQLDisabled();
}

void WKPreferencesSetCaptureAudioInUIProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCaptureAudioInUIProcessEnabled(flag);
}

bool WKPreferencesGetCaptureAudioInUIProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->captureAudioInUIProcessEnabled();
}

void WKPreferencesSetCaptureVideoInUIProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setCaptureVideoInUIProcessEnabled(flag);
}

bool WKPreferencesGetCaptureVideoInUIProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->captureVideoInUIProcessEnabled();
}

void WKPreferencesSetReferrerPolicyAttributeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toImpl(preferencesRef)->setReferrerPolicyAttributeEnabled(flag);
}

bool WKPreferencesGetReferrerPolicyAttributeEnabled(WKPreferencesRef preferencesRef)
{
    return toImpl(preferencesRef)->referrerPolicyAttributeEnabled();
}
