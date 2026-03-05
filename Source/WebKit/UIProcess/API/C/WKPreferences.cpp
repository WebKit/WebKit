/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "APIArray.h"
#include "WKAPICast.h"
#include "WKPreferencesRef.h"
#include "WKPreferencesRefPrivate.h"
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
    return toAPILeakingRef(WebPreferences::createWithLegacyDefaults(String(), "WebKit2."_s, "WebKit2."_s));
}

WKPreferencesRef WKPreferencesCreateWithIdentifier(WKStringRef identifierRef)
{
    return toAPILeakingRef(WebPreferences::createWithLegacyDefaults(toWTFString(identifierRef), "WebKit2."_s, "WebKit2."_s));
}

WKPreferencesRef WKPreferencesCreateCopy(WKPreferencesRef preferencesRef)
{
    return toAPILeakingRef(protect(toImpl(preferencesRef))->copy());
}

void WKPreferencesStartBatchingUpdates(WKPreferencesRef preferencesRef)
{
    protect(toImpl(preferencesRef))->startBatchingUpdates();
}

void WKPreferencesEndBatchingUpdates(WKPreferencesRef preferencesRef)
{
    protect(toImpl(preferencesRef))->endBatchingUpdates();
}

WKArrayRef WKPreferencesCopyExperimentalFeatures(WKPreferencesRef preferencesRef)
{
    auto experimentalFeatures = toImpl(preferencesRef)->experimentalFeatures();
    return toAPILeakingRef(API::Array::create(WTF::move(experimentalFeatures)));
}

void WKPreferencesEnableAllExperimentalFeatures(WKPreferencesRef preferencesRef)
{
    protect(toImpl(preferencesRef))->enableAllExperimentalFeatures();
}

void WKPreferencesSetExperimentalFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef experimentalFeatureKey)
{
    protect(toImpl(preferencesRef))->setFeatureEnabledForKey(toWTFString(experimentalFeatureKey), value);
}

WKArrayRef WKPreferencesCopyInternalDebugFeatures(WKPreferencesRef preferencesRef)
{
    auto internalDebugFeatures = toImpl(preferencesRef)->internalDebugFeatures();
    return toAPILeakingRef(API::Array::create(WTF::move(internalDebugFeatures)));
}

void WKPreferencesResetAllInternalDebugFeatures(WKPreferencesRef preferencesRef)
{
    protect(toImpl(preferencesRef))->resetAllInternalDebugFeatures();
}

void WKPreferencesSetInternalDebugFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef internalDebugFeatureKey)
{
    protect(toImpl(preferencesRef))->setFeatureEnabledForKey(toWTFString(internalDebugFeatureKey), value);
}

void WKPreferencesSetBoolValueForKeyForTesting(WKPreferencesRef preferencesRef, bool value, WKStringRef key)
{
    protect(toImpl(preferencesRef))->setBoolValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetDoubleValueForKeyForTesting(WKPreferencesRef preferencesRef, double value, WKStringRef key)
{
    protect(toImpl(preferencesRef))->setBoolValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetUInt32ValueForKeyForTesting(WKPreferencesRef preferencesRef, uint32_t value, WKStringRef key)
{
    protect(toImpl(preferencesRef))->setUInt32ValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetStringValueForKeyForTesting(WKPreferencesRef preferencesRef, WKStringRef value, WKStringRef key)
{
    protect(toImpl(preferencesRef))->setStringValueForKey(toWTFString(key), toWTFString(value), true);
}

void WKPreferencesResetTestRunnerOverrides(WKPreferencesRef preferencesRef)
{
    // Currently we reset the overrides on the web process when preferencesDidChange() is called. Since WTR preferences
    // are usually always the same (in the UI process), they are not sent to web process, not triggering the reset.
    protect(toImpl(preferencesRef))->forceUpdate();
}

void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferencesRef, bool javaScriptEnabled)
{
    protect(toImpl(preferencesRef))->setJavaScriptEnabled(javaScriptEnabled);
}

bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->javaScriptEnabled();
}

void WKPreferencesSetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef, bool javaScriptMarkupEnabled)
{
    protect(toImpl(preferencesRef))->setJavaScriptMarkupEnabled(javaScriptMarkupEnabled);
}

bool WKPreferencesGetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->javaScriptMarkupEnabled();
}

void WKPreferencesSetLoadsImagesAutomatically(WKPreferencesRef preferencesRef, bool loadsImagesAutomatically)
{
    protect(toImpl(preferencesRef))->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

bool WKPreferencesGetLoadsImagesAutomatically(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->loadsImagesAutomatically();
}

void WKPreferencesSetLocalStorageEnabled(WKPreferencesRef preferencesRef, bool localStorageEnabled)
{
    protect(toImpl(preferencesRef))->setLocalStorageEnabled(localStorageEnabled);
}

bool WKPreferencesGetLocalStorageEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->localStorageEnabled();
}

void WKPreferencesSetDatabasesEnabled(WKPreferencesRef preferencesRef, bool databasesEnabled)
{
    protect(toImpl(preferencesRef))->setDatabasesEnabled(databasesEnabled);
}

bool WKPreferencesGetDatabasesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->databasesEnabled();
}

void WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef, bool javaScriptCanOpenWindowsAutomatically)
{
    protect(toImpl(preferencesRef))->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

bool WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->javaScriptCanOpenWindowsAutomatically();
}

void WKPreferencesSetStandardFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setStandardFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyStandardFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->standardFontFamily());
}

void WKPreferencesSetFixedFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setFixedFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFixedFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->fixedFontFamily());
}

void WKPreferencesSetSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->serifFontFamily());
}

void WKPreferencesSetSansSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setSansSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySansSerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->sansSerifFontFamily());
}

void WKPreferencesSetCursiveFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setCursiveFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyCursiveFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->cursiveFontFamily());
}

void WKPreferencesSetFantasyFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setFantasyFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFantasyFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->fantasyFontFamily());
}

void WKPreferencesSetPictographFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setPictographFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyPictographFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->pictographFontFamily());
}

void WKPreferencesSetMathFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    protect(toImpl(preferencesRef))->setMathFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyMathFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->mathFontFamily());
}

void WKPreferencesSetDefaultFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    protect(toImpl(preferencesRef))->setDefaultFontSize(size);
}

uint32_t WKPreferencesGetDefaultFontSize(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->defaultFontSize();
}

void WKPreferencesSetDefaultFixedFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    protect(toImpl(preferencesRef))->setDefaultFixedFontSize(size);
}

uint32_t WKPreferencesGetDefaultFixedFontSize(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->defaultFixedFontSize();
}

void WKPreferencesSetMinimumFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    protect(toImpl(preferencesRef))->setMinimumFontSize(size);
}

uint32_t WKPreferencesGetMinimumFontSize(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->minimumFontSize();
}


void WKPreferencesSetCookieEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setCookieEnabled(enabled);
}

bool WKPreferencesGetCookieEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->cookieEnabled();
}

void WKPreferencesSetEditableLinkBehavior(WKPreferencesRef preferencesRef, WKEditableLinkBehavior wkBehavior)
{
    protect(toImpl(preferencesRef))->setEditableLinkBehavior(static_cast<uint32_t>(toEditableLinkBehavior(wkBehavior)));
}

WKEditableLinkBehavior WKPreferencesGetEditableLinkBehavior(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(protect(toImpl(preferencesRef))->editableLinkBehavior()));
}

void WKPreferencesSetDefaultTextEncodingName(WKPreferencesRef preferencesRef, WKStringRef name)
{
    protect(toImpl(preferencesRef))->setDefaultTextEncodingName(toWTFString(name));
}

WKStringRef WKPreferencesCopyDefaultTextEncodingName(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->defaultTextEncodingName());
}

void WKPreferencesSetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setDeveloperExtrasEnabled(enabled);
}

bool WKPreferencesGetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->developerExtrasEnabled();
}

void WKPreferencesSetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef, WKJavaScriptRuntimeFlagSet javaScriptRuntimeFlagSet)
{
    protect(toImpl(preferencesRef))->setJavaScriptRuntimeFlags(javaScriptRuntimeFlagSet);
}

WKJavaScriptRuntimeFlagSet WKPreferencesGetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->javaScriptRuntimeFlags();
}

void WKPreferencesSetTextAreasAreResizable(WKPreferencesRef preferencesRef, bool resizable)
{
    protect(toImpl(preferencesRef))->setTextAreasAreResizable(resizable);
}

bool WKPreferencesGetTextAreasAreResizable(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->textAreasAreResizable();
}

void WKPreferencesSetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setAcceleratedDrawingEnabled(flag);
}

bool WKPreferencesGetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->acceleratedDrawingEnabled();
}

void WKPreferencesSetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCanvasUsesAcceleratedDrawing(flag);
}

bool WKPreferencesGetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->canvasUsesAcceleratedDrawing();
}

void WKPreferencesSetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setAcceleratedCompositingEnabled(flag);
}

bool WKPreferencesGetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->acceleratedCompositingEnabled();
}

void WKPreferencesSetCompositingBordersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCompositingBordersVisible(flag);
}

bool WKPreferencesGetCompositingBordersVisible(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->compositingBordersVisible();
}

void WKPreferencesSetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCompositingRepaintCountersVisible(flag);
}

bool WKPreferencesGetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->compositingRepaintCountersVisible();
}

void WKPreferencesSetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setTiledScrollingIndicatorVisible(flag);
}

bool WKPreferencesGetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->tiledScrollingIndicatorVisible();
}

void WKPreferencesSetWebGLEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setWebGLEnabled(flag);
}

bool WKPreferencesGetWebGLEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webGLEnabled();
}

void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setNeedsSiteSpecificQuirks(flag);
}

bool WKPreferencesGetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->needsSiteSpecificQuirks();
}

void WKPreferencesSetForceFTPDirectoryListings(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setForceFTPDirectoryListings(flag);
}

bool WKPreferencesGetForceFTPDirectoryListings(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->forceFTPDirectoryListings();
}

void WKPreferencesSetFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef, WKStringRef pathRef)
{
    protect(toImpl(preferencesRef))->setFTPDirectoryTemplatePath(toWTFString(pathRef));
}

WKStringRef WKPreferencesCopyFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->ftpDirectoryTemplatePath());
}

void WKPreferencesSetTabsToLinks(WKPreferencesRef preferencesRef, bool tabsToLinks)
{
    protect(toImpl(preferencesRef))->setTabsToLinks(tabsToLinks);
}

bool WKPreferencesGetTabsToLinks(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->tabsToLinks();
}

void WKPreferencesSetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAuthorAndUserStylesEnabled(enabled);
}

bool WKPreferencesGetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->authorAndUserStylesEnabled();
}

void WKPreferencesSetShouldPrintBackgrounds(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setShouldPrintBackgrounds(flag);
}

bool WKPreferencesGetShouldPrintBackgrounds(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldPrintBackgrounds();
}

void WKPreferencesSetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setDOMTimersThrottlingEnabled(enabled);
}

bool WKPreferencesGetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->domTimersThrottlingEnabled();
}

void WKPreferencesSetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setWebArchiveDebugModeEnabled(enabled);
}

bool WKPreferencesGetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webArchiveDebugModeEnabled();
}

void WKPreferencesSetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setLocalFileContentSniffingEnabled(enabled);
}

bool WKPreferencesGetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->localFileContentSniffingEnabled();
}

void WKPreferencesSetPageCacheEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setUsesBackForwardCache(enabled);
}

bool WKPreferencesGetPageCacheEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->usesBackForwardCache();
}

void WKPreferencesSetDOMPasteAllowed(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setDOMPasteAllowed(enabled);
}

bool WKPreferencesGetDOMPasteAllowed(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->domPasteAllowed();
}

void WKPreferencesSetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setJavaScriptCanAccessClipboard(enabled);
}

bool WKPreferencesGetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->javaScriptCanAccessClipboard();
}

void WKPreferencesSetFullScreenEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setFullScreenEnabled(enabled);
}

bool WKPreferencesGetFullScreenEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->fullScreenEnabled();
}

void WKPreferencesSetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAsynchronousSpellCheckingEnabled(enabled);
}

bool WKPreferencesGetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->asynchronousSpellCheckingEnabled();
}

void WKPreferencesSetAVFoundationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAVFoundationEnabled(enabled);
}

bool WKPreferencesGetAVFoundationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->isAVFoundationEnabled();
}

void WKPreferencesSetWebSecurityEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setWebSecurityEnabled(enabled);
}

bool WKPreferencesGetWebSecurityEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webSecurityEnabled();
}

void WKPreferencesSetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    protect(toImpl(preferencesRef))->setAllowUniversalAccessFromFileURLs(allowed);
}

bool WKPreferencesGetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowUniversalAccessFromFileURLs();
}

void WKPreferencesSetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    protect(toImpl(preferencesRef))->setAllowFileAccessFromFileURLs(allowed);
}

bool WKPreferencesGetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowFileAccessFromFileURLs();
}

void WKPreferencesSetTopNavigationToDataURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    protect(toImpl(preferencesRef))->setAllowTopNavigationToDataURLs(allowed);
}

bool WKPreferencesGetTopNavigationToDataURLsAllowed(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowTopNavigationToDataURLs();
}

void WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef, bool needsQuirk)
{
    protect(toImpl(preferencesRef))->setNeedsStorageAccessFromFileURLsQuirk(needsQuirk);
}

bool WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->needsStorageAccessFromFileURLsQuirk();
}

void WKPreferencesSetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setRequiresUserGestureForMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->requiresUserGestureForMediaPlayback();
}

void WKPreferencesSetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setRequiresUserGestureForVideoPlayback(flag);
}

bool WKPreferencesGetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->requiresUserGestureForVideoPlayback();
}

void WKPreferencesSetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setRequiresUserGestureForAudioPlayback(flag);
}

bool WKPreferencesGetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->requiresUserGestureForAudioPlayback();
}

void WKPreferencesSetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setMainContentUserGestureOverrideEnabled(flag);
}

bool WKPreferencesGetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mainContentUserGestureOverrideEnabled();
}

bool WKPreferencesGetVerifyUserGestureInUIProcessEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->verifyWindowOpenUserGestureFromUIProcess();
}

void WKPreferencesSetManagedMediaSourceLowThreshold(WKPreferencesRef preferencesRef, double threshold)
{
    protect(toImpl(preferencesRef))->setManagedMediaSourceLowThreshold(threshold);
}

double WKPreferencesGetManagedMediaSourceLowThreshold(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->managedMediaSourceLowThreshold();
}

void WKPreferencesSetManagedMediaSourceHighThreshold(WKPreferencesRef preferencesRef, double threshold)
{
    protect(toImpl(preferencesRef))->setManagedMediaSourceHighThreshold(threshold);
}

double WKPreferencesGetManagedMediaSourceHighThreshold(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->managedMediaSourceHighThreshold();
}

void WKPreferencesSetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setAllowsInlineMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowsInlineMediaPlayback();
}

void WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setInlineMediaPlaybackRequiresPlaysInlineAttribute(flag);
}

bool WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

void WKPreferencesSetBeaconAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setBeaconAPIEnabled(flag);
}

bool WKPreferencesGetBeaconAPIEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->beaconAPIEnabled();
}

void WKPreferencesSetDirectoryUploadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setDirectoryUploadEnabled(flag);
}

bool WKPreferencesGetDirectoryUploadEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->directoryUploadEnabled();
}

void WKPreferencesSetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setMediaControlsScaleWithPageZoom(flag);
}

bool WKPreferencesGetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaControlsScaleWithPageZoom();
}

void WKPreferencesSetWebAuthenticationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setWebAuthenticationEnabled(flag);
}

bool WKPreferencesGetWebAuthenticationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webAuthenticationEnabled();
}

void WKPreferencesSetDigitalCredentialsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setDigitalCredentialsEnabled(flag);
}

bool WKPreferencesGetDigitalCredentialsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->digitalCredentialsEnabled();
}

void WKPreferencesSetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setInvisibleAutoplayNotPermitted(!flag);
}

bool WKPreferencesGetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef)
{
    return !protect(toImpl(preferencesRef))->invisibleAutoplayNotPermitted();
}

void WKPreferencesSetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setShowsToolTipOverTruncatedText(flag);
}

bool WKPreferencesGetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->showsToolTipOverTruncatedText();
}

void WKPreferencesSetMockScrollbarsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setMockScrollbarsEnabled(flag);
}

bool WKPreferencesGetMockScrollbarsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mockScrollbarsEnabled();
}

void WKPreferencesSetAttachmentElementEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setAttachmentElementEnabled(flag);
}

bool WKPreferencesGetAttachmentElementEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->attachmentElementEnabled();
}

void WKPreferencesSetWebAudioEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setWebAudioEnabled(enabled);
}

bool WKPreferencesGetWebAudioEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webAudioEnabled();
}

void WKPreferencesSetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setSuppressesIncrementalRendering(enabled);
}

bool WKPreferencesGetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->suppressesIncrementalRendering();
}

void WKPreferencesSetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setBackspaceKeyNavigationEnabled(enabled);
}

bool WKPreferencesGetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->backspaceKeyNavigationEnabled();
}

void WKPreferencesSetCaretBrowsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setCaretBrowsingEnabled(enabled);
}

bool WKPreferencesGetCaretBrowsingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->caretBrowsingEnabled();
}

void WKPreferencesSetShouldDisplaySubtitles(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setShouldDisplaySubtitles(enabled);
}

bool WKPreferencesGetShouldDisplaySubtitles(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldDisplaySubtitles();
}

void WKPreferencesSetShouldDisplayCaptions(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setShouldDisplayCaptions(enabled);
}

bool WKPreferencesGetShouldDisplayCaptions(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldDisplayCaptions();
}

void WKPreferencesSetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setShouldDisplayTextDescriptions(enabled);
}

bool WKPreferencesGetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldDisplayTextDescriptions();
}

void WKPreferencesSetNotificationsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setNotificationsEnabled(enabled);
}

bool WKPreferencesGetNotificationsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->notificationsEnabled();
}

void WKPreferencesSetShouldRespectImageOrientation(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setShouldRespectImageOrientation(enabled);
}

bool WKPreferencesGetShouldRespectImageOrientation(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldRespectImageOrientation();
}

void WKPreferencesSetStorageBlockingPolicy(WKPreferencesRef preferencesRef, WKStorageBlockingPolicy policy)
{
    protect(toImpl(preferencesRef))->setStorageBlockingPolicy(static_cast<uint32_t>(toStorageBlockingPolicy(policy)));
}

WKStorageBlockingPolicy WKPreferencesGetStorageBlockingPolicy(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::StorageBlockingPolicy>(protect(toImpl(preferencesRef))->storageBlockingPolicy()));
}

void WKPreferencesSetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setDiagnosticLoggingEnabled(enabled);
}

bool WKPreferencesGetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->diagnosticLoggingEnabled();
}

void WKPreferencesSetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setInteractiveFormValidationEnabled(enabled);
}

bool WKPreferencesGetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->interactiveFormValidationEnabled();
}

void WKPreferencesSetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setScrollingPerformanceTestingEnabled(enabled);
}

bool WKPreferencesGetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->scrollingPerformanceTestingEnabled();
}

void WKPreferencesSetPDFPluginEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setPDFPluginEnabled(enabled);
}

bool WKPreferencesGetPDFPluginEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->pdfPluginEnabled();
}

void WKPreferencesSetEncodingDetectorEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setUsesEncodingDetector(enabled);
}

bool WKPreferencesGetEncodingDetectorEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->usesEncodingDetector();
}

void WKPreferencesSetTextAutosizingEnabled(WKPreferencesRef preferencesRef, bool textAutosizingEnabled)
{
    protect(toImpl(preferencesRef))->setTextAutosizingEnabled(textAutosizingEnabled);
}

bool WKPreferencesGetTextAutosizingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->textAutosizingEnabled();
}

void WKPreferencesSetTextAutosizingUsesIdempotentMode(WKPreferencesRef preferencesRef, bool textAutosizingUsesIdempotentModeEnabled)
{
    protect(toImpl(preferencesRef))->setTextAutosizingUsesIdempotentMode(textAutosizingUsesIdempotentModeEnabled);
}

bool WKPreferencesGetTextAutosizingUsesIdempotentMode(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->textAutosizingUsesIdempotentMode();
}

void WKPreferencesSetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAggressiveTileRetentionEnabled(enabled);
}

bool WKPreferencesGetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->aggressiveTileRetentionEnabled();
}

void WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setLogsPageMessagesToSystemConsoleEnabled(enabled);
}

bool WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->logsPageMessagesToSystemConsoleEnabled();
}

void WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

bool WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->pageVisibilityBasedProcessSuppressionEnabled();
}

void WKPreferencesSetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setSmartInsertDeleteEnabled(enabled);
}

bool WKPreferencesGetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->smartInsertDeleteEnabled();
}

void WKPreferencesSetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setSelectTrailingWhitespaceEnabled(enabled);
}

bool WKPreferencesGetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->selectTrailingWhitespaceEnabled();
}

void WKPreferencesSetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setShowsURLsInToolTipsEnabled(enabled);
}

bool WKPreferencesGetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->showsURLsInToolTipsEnabled();
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setHiddenPageDOMTimerThrottlingEnabled(enabled);
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setHiddenPageDOMTimerThrottlingAutoIncreases(enabled);
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->hiddenPageDOMTimerThrottlingEnabled();
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->hiddenPageDOMTimerThrottlingAutoIncreases();
}

void WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setHiddenPageCSSAnimationSuspensionEnabled(enabled);
}

bool WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->hiddenPageCSSAnimationSuspensionEnabled();
}

void WKPreferencesSetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef, double timeout)
{
    protect(toImpl(preferencesRef))->setIncrementalRenderingSuppressionTimeout(timeout);
}

double WKPreferencesGetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->incrementalRenderingSuppressionTimeout();
}

void WKPreferencesSetThreadedScrollingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setThreadedScrollingEnabled(enabled);
}

bool WKPreferencesGetThreadedScrollingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->threadedScrollingEnabled();
}

void WKPreferencesSetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setLegacyLineLayoutVisualCoverageEnabled(flag);
}

bool WKPreferencesGetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->legacyLineLayoutVisualCoverageEnabled();
}

void WKPreferencesSetContentChangeObserverEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setContentChangeObserverEnabled(flag);
}

bool WKPreferencesGetContentChangeObserverEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->contentChangeObserverEnabled();
}

void WKPreferencesSetUseGiantTiles(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setUseGiantTiles(flag);
}

bool WKPreferencesGetUseGiantTiles(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->useGiantTiles();
}

void WKPreferencesSetMediaDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setMediaDevicesEnabled(enabled);
}

bool WKPreferencesGetMediaDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaDevicesEnabled();
}

void WKPreferencesSetPeerConnectionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setPeerConnectionEnabled(enabled);
}

bool WKPreferencesGetPeerConnectionEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->peerConnectionEnabled();
}

void WKPreferencesSetSpatialNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setSpatialNavigationEnabled(enabled);
}

bool WKPreferencesGetSpatialNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->spatialNavigationEnabled();
}

void WKPreferencesSetMediaSourceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setMediaSourceEnabled(enabled);
}

bool WKPreferencesGetMediaSourceEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaSourceEnabled();
}

void WKPreferencesSetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setSourceBufferChangeTypeEnabled(enabled);
}

bool WKPreferencesGetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->sourceBufferChangeTypeEnabled();
}

void WKPreferencesSetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setViewGestureDebuggingEnabled(enabled);
}

bool WKPreferencesGetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->viewGestureDebuggingEnabled();
}

void WKPreferencesSetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef, bool convert)
{
    protect(toImpl(preferencesRef))->setShouldConvertPositionStyleOnCopy(convert);
}

bool WKPreferencesGetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldConvertPositionStyleOnCopy();
}

void WKPreferencesSetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setTelephoneNumberParsingEnabled(enabled);
}

bool WKPreferencesGetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->telephoneNumberParsingEnabled();
}

void WKPreferencesSetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setEnableInheritURIQueryComponent(enabled);
}

bool WKPreferencesGetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->enableInheritURIQueryComponent();
}

void WKPreferencesSetServiceControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setServiceControlsEnabled(enabled);
}

bool WKPreferencesGetServiceControlsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->serviceControlsEnabled();
}

void WKPreferencesSetImageControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setImageControlsEnabled(enabled);
}

bool WKPreferencesGetImageControlsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->imageControlsEnabled();
}

void WKPreferencesSetGamepadsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setGamepadsEnabled(enabled);
}

bool WKPreferencesGetGamepadsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->gamepadsEnabled();
}

void WKPreferencesSetMinimumZoomFontSize(WKPreferencesRef preferencesRef, double size)
{
    protect(toImpl(preferencesRef))->setMinimumZoomFontSize(size);
}

double WKPreferencesGetMinimumZoomFontSize(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->minimumZoomFontSize();
}

void WKPreferencesSetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef, WKDebugOverlayRegions visibleRegions)
{
    protect(toImpl(preferencesRef))->setVisibleDebugOverlayRegions(visibleRegions);
}

WKDebugOverlayRegions WKPreferencesGetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->visibleDebugOverlayRegions();
}

void WKPreferencesSetMetaRefreshEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetMetaRefreshEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->httpEquivEnabled();
}

void WKPreferencesSetHTTPEquivEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetHTTPEquivEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->httpEquivEnabled();
}

void WKPreferencesSetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAllowsAirPlayForMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowsAirPlayForMediaPlayback();
}

void WKPreferencesSetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef, _WKUserInterfaceDirectionPolicy userInterfaceDirectionPolicy)
{
    protect(toImpl(preferencesRef))->setUserInterfaceDirectionPolicy(userInterfaceDirectionPolicy);
}

_WKUserInterfaceDirectionPolicy WKPreferencesGetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef)
{
    return static_cast<_WKUserInterfaceDirectionPolicy>(protect(toImpl(preferencesRef))->userInterfaceDirectionPolicy());
}

void WKPreferencesSetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setResourceUsageOverlayVisible(enabled);
}

bool WKPreferencesGetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->resourceUsageOverlayVisible();
}

void WKPreferencesSetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setMockCaptureDevicesEnabled(enabled);
}

bool WKPreferencesGetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mockCaptureDevicesEnabled();
}

void WKPreferencesSetGetUserMediaRequiresFocus(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setGetUserMediaRequiresFocus(enabled);
}

bool WKPreferencesGetGetUserMediaRequiresFocus(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->getUserMediaRequiresFocus();
}

void WKPreferencesSetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setICECandidateFilteringEnabled(enabled);
}

bool WKPreferencesGetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->iceCandidateFilteringEnabled();
}

void WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

bool WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->enumeratingAllNetworkInterfacesEnabled();
}

void WKPreferencesSetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setMediaCaptureRequiresSecureConnection(enabled);
}

bool WKPreferencesGetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaCaptureRequiresSecureConnection();
}

void WKPreferencesSetInactiveMediaCaptureStreamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef, double interval)
{
    protect(toImpl(preferencesRef))->setInactiveMediaCaptureStreamRepromptIntervalInMinutes(interval);
}

double WKPreferencesGetInactiveMediaCaptureStreamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->inactiveMediaCaptureStreamRepromptIntervalInMinutes();
}

void WKPreferencesSetDataTransferItemsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setDataTransferItemsEnabled(flag);
}

bool WKPreferencesGetDataTransferItemsEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->dataTransferItemsEnabled();
}

void WKPreferencesSetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCustomPasteboardDataEnabled(flag);
}

bool WKPreferencesGetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->customPasteboardDataEnabled();
}

void WKPreferencesSetWriteRichTextDataWhenCopyingOrDragging(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setWriteRichTextDataWhenCopyingOrDragging(flag);
}

bool WKPreferencesGetWriteRichTextDataWhenCopyingOrDragging(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->writeRichTextDataWhenCopyingOrDragging();
}

void WKPreferencesSetWebShareEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setWebShareEnabled(flag);
}

bool WKPreferencesGetWebShareEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webShareEnabled();
}

void WKPreferencesSetDownloadAttributeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setDownloadAttributeEnabled(flag);
}

bool WKPreferencesGetDownloadAttributeEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->downloadAttributeEnabled();
}

void WKPreferencesSetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setWebRTCPlatformCodecsInGPUProcessEnabled(flag);
}

bool WKPreferencesGetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->webRTCPlatformCodecsInGPUProcessEnabled();
}

WK_EXPORT void WKPreferencesSetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setIsAccessibilityIsolatedTreeEnabled(flag);
}

WK_EXPORT bool WKPreferencesGetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->isAccessibilityIsolatedTreeEnabled();
}

void WKPreferencesSetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setAllowsPictureInPictureMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowsPictureInPictureMediaPlayback();
}

WK_EXPORT bool WKPreferencesGetApplePayEnabled(WKPreferencesRef preferencesRef)
{
    return protect(WebKit::toImpl(preferencesRef))->applePayEnabled();
}

void WKPreferencesSetApplePayEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(WebKit::toImpl(preferencesRef))->setApplePayEnabled(enabled);
}

bool WKPreferencesGetCSSTransformStyleSeparatedEnabled(WKPreferencesRef preferencesRef)
{
    return protect(WebKit::toImpl(preferencesRef))->cssTransformStyleSeparatedEnabled();
}

void WKPreferencesSetCSSTransformStyleSeparatedEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(WebKit::toImpl(preferencesRef))->setCSSTransformStyleSeparatedEnabled(enabled);
}

bool WKPreferencesGetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef)
{
    return protect(WebKit::toImpl(preferencesRef))->applePayCapabilityDisclosureAllowed();
}

void WKPreferencesSetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    protect(WebKit::toImpl(preferencesRef))->setApplePayCapabilityDisclosureAllowed(allowed);
}

void WKPreferencesSetLinkPreloadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setLinkPreloadEnabled(flag);
}

bool WKPreferencesGetLinkPreloadEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->linkPreloadEnabled();
}

void WKPreferencesSetMediaPreloadingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setMediaPreloadingEnabled(flag);
}

bool WKPreferencesGetMediaPreloadingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaPreloadingEnabled();
}

void WKPreferencesSetExposeSpeakersEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setExposeSpeakersEnabled(flag);
}

bool WKPreferencesGetExposeSpeakersEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->exposeSpeakersEnabled();
}

void WKPreferencesSetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setLargeImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->largeImageAsyncDecodingEnabled();
}

void WKPreferencesSetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setAnimatedImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->animatedImageAsyncDecodingEnabled();
}

void WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(flag);
}

bool WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

void WKPreferencesSetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setMediaUserGestureInheritsFromDocument(flag);
}

bool WKPreferencesGetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaUserGestureInheritsFromDocument();
}

void WKPreferencesSetMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef, WKStringRef codecs)
{
    protect(toImpl(preferencesRef))->setMediaContentTypesRequiringHardwareSupport(toWTFString(codecs));
}

WKStringRef WKPreferencesCopyMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(protect(toImpl(preferencesRef))->mediaContentTypesRequiringHardwareSupport());
}

bool WKPreferencesGetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->legacyEncryptedMediaAPIEnabled();
}

void WKPreferencesSetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    return protect(toImpl(preferencesRef))->setLegacyEncryptedMediaAPIEnabled(enabled);
}

bool WKPreferencesGetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->allowMediaContentTypesRequiringHardwareSupportAsFallback();
}

void WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef, bool allow)
{
    return protect(toImpl(preferencesRef))->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(allow);
}

void WKPreferencesSetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setShouldAllowUserInstalledFonts(flag);
}

bool WKPreferencesGetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->shouldAllowUserInstalledFonts();
}

void WKPreferencesSetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setMediaCapabilitiesEnabled(enabled);
}

bool WKPreferencesGetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->mediaCapabilitiesEnabled();
}

void WKPreferencesSetColorFilterEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setColorFilterEnabled(flag);
}

bool WKPreferencesGetColorFilterEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->colorFilterEnabled();
}

void WKPreferencesSetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setProcessSwapOnCrossSiteNavigationEnabled(flag);
}

bool WKPreferencesGetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->processSwapOnCrossSiteNavigationEnabled();
}

void WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setPunchOutWhiteBackgroundsInDarkMode(flag);
}

bool WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->punchOutWhiteBackgroundsInDarkMode();
}

void WKPreferencesSetCaptureAudioInUIProcessEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetCaptureAudioInUIProcessEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetCaptureAudioInGPUProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCaptureAudioInGPUProcessEnabled(flag);
}

bool WKPreferencesGetCaptureAudioInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->captureAudioInGPUProcessEnabled();
}

void WKPreferencesSetCaptureVideoInUIProcessEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetCaptureVideoInUIProcessEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetCaptureVideoInGPUProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setCaptureVideoInGPUProcessEnabled(flag);
}

bool WKPreferencesGetCaptureVideoInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->captureVideoInGPUProcessEnabled();
}

void WKPreferencesSetVP9DecoderEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    protect(toImpl(preferencesRef))->setVP9DecoderEnabled(flag);
}

bool WKPreferencesGetVP9DecoderEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->vp9DecoderEnabled();
}

bool WKPreferencesGetRemotePlaybackEnabled(WKPreferencesRef preferencesRef)
{
    return protect(WebKit::toImpl(preferencesRef))->remotePlaybackEnabled();
}

void WKPreferencesSetRemotePlaybackEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(WebKit::toImpl(preferencesRef))->setRemotePlaybackEnabled(enabled);
}

bool WKPreferencesGetShouldUseServiceWorkerShortTimeout(WKPreferencesRef preferencesRef)
{
    return protect(WebKit::toImpl(preferencesRef))->shouldUseServiceWorkerShortTimeout();
}

void WKPreferencesSetShouldUseServiceWorkerShortTimeout(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(WebKit::toImpl(preferencesRef))->setShouldUseServiceWorkerShortTimeout(enabled);
}

void WKPreferencesSetRequestVideoFrameCallbackEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    protect(toImpl(preferencesRef))->setRequestVideoFrameCallbackEnabled(enabled);
}

bool WKPreferencesGetRequestVideoFrameCallbackEnabled(WKPreferencesRef preferencesRef)
{
    return protect(toImpl(preferencesRef))->requestVideoFrameCallbackEnabled();
}


// The following are all deprecated and do nothing. They should be removed when possible.

void WKPreferencesSetCSSOMViewScrollingAPIEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetCSSOMViewScrollingAPIEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetHyperlinkAuditingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetHyperlinkAuditingEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetDNSPrefetchingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetDNSPrefetchingEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetRestrictedHTTPResponseAccess(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetRestrictedHTTPResponseAccess(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetPluginsEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPluginsEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPageCacheSupportsPlugins(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPageCacheSupportsPlugins(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetAsynchronousPluginInitializationEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAsynchronousPluginInitializationEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetArtificialPluginInitializationDelayEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetArtificialPluginInitializationDelayEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPlugInSnapshottingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPlugInSnapshottingEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetSnapshotAllPlugIns(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetSnapshotAllPlugIns(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPaintTimingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPaintTimingEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetRequestAnimationFrameEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetRequestAnimationFrameEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetAVFoundationNSURLSessionEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAVFoundationNSURLSessionEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetStorageAccessAPIEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetStorageAccessAPIEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetPrivateBrowsingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPrivateBrowsingEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetIgnoreViewportScalingConstraints(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetIgnoreViewportScalingConstraints(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetKeygenElementEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetKeygenElementEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetNewBlockInsideInlineModelEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetNewBlockInsideInlineModelEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetLongMousePressEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetLongMousePressEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetAntialiasedFontDilationEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetAntialiasedFontDilationEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetHixie76WebSocketProtocolEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetHixie76WebSocketProtocolEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetFetchAPIEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetFetchAPIEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetFetchAPIKeepAliveEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetFetchAPIKeepAliveEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetIntersectionObserverEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetIntersectionObserverEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetIsSecureContextAttributeEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetIsSecureContextAttributeEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetUserTimingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetUserTimingEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetResourceTimingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetResourceTimingEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetCrossOriginResourcePolicyEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetCrossOriginResourcePolicyEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetHighlightAPIEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
}

bool WKPreferencesGetHighlightAPIEnabled(WKPreferencesRef preferencesRef)
{
    return true;
}

void WKPreferencesSetWebSQLDisabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetWebSQLDisabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetXSSAuditorEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetXSSAuditorEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetJavaEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetJavaEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetPaginateDuringLayoutEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetPaginateDuringLayoutEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetLoadsSiteIconsIgnoringImageLoadingPreference(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetLoadsSiteIconsIgnoringImageLoadingPreference(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetMenuItemElementEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetMenuItemElementEnabled(WKPreferencesRef)
{
    return false;
}

void WKPreferencesSetSyntheticEditingCommandsEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetSyntheticEditingCommandsEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetReferrerPolicyAttributeEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetReferrerPolicyAttributeEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetServerTimingEnabled(WKPreferencesRef, bool)
{
}

bool WKPreferencesGetServerTimingEnabled(WKPreferencesRef)
{
    return true;
}

void WKPreferencesSetMediaStreamEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
}

bool WKPreferencesGetMediaStreamEnabled(WKPreferencesRef preferencesRef)
{
    return true;
}
