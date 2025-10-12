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
    return toAPILeakingRef(toProtectedImpl(preferencesRef)->copy());
}

void WKPreferencesStartBatchingUpdates(WKPreferencesRef preferencesRef)
{
    toProtectedImpl(preferencesRef)->startBatchingUpdates();
}

void WKPreferencesEndBatchingUpdates(WKPreferencesRef preferencesRef)
{
    toProtectedImpl(preferencesRef)->endBatchingUpdates();
}

WKArrayRef WKPreferencesCopyExperimentalFeatures(WKPreferencesRef preferencesRef)
{
    auto experimentalFeatures = toImpl(preferencesRef)->experimentalFeatures();
    return toAPILeakingRef(API::Array::create(WTFMove(experimentalFeatures)));
}

void WKPreferencesEnableAllExperimentalFeatures(WKPreferencesRef preferencesRef)
{
    toProtectedImpl(preferencesRef)->enableAllExperimentalFeatures();
}

void WKPreferencesSetExperimentalFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef experimentalFeatureKey)
{
    toProtectedImpl(preferencesRef)->setFeatureEnabledForKey(toWTFString(experimentalFeatureKey), value);
}

WKArrayRef WKPreferencesCopyInternalDebugFeatures(WKPreferencesRef preferencesRef)
{
    auto internalDebugFeatures = toImpl(preferencesRef)->internalDebugFeatures();
    return toAPILeakingRef(API::Array::create(WTFMove(internalDebugFeatures)));
}

void WKPreferencesResetAllInternalDebugFeatures(WKPreferencesRef preferencesRef)
{
    toProtectedImpl(preferencesRef)->resetAllInternalDebugFeatures();
}

void WKPreferencesSetInternalDebugFeatureForKey(WKPreferencesRef preferencesRef, bool value, WKStringRef internalDebugFeatureKey)
{
    toProtectedImpl(preferencesRef)->setFeatureEnabledForKey(toWTFString(internalDebugFeatureKey), value);
}

void WKPreferencesSetBoolValueForKeyForTesting(WKPreferencesRef preferencesRef, bool value, WKStringRef key)
{
    toProtectedImpl(preferencesRef)->setBoolValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetDoubleValueForKeyForTesting(WKPreferencesRef preferencesRef, double value, WKStringRef key)
{
    toProtectedImpl(preferencesRef)->setBoolValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetUInt32ValueForKeyForTesting(WKPreferencesRef preferencesRef, uint32_t value, WKStringRef key)
{
    toProtectedImpl(preferencesRef)->setUInt32ValueForKey(toWTFString(key), value, true);
}

void WKPreferencesSetStringValueForKeyForTesting(WKPreferencesRef preferencesRef, WKStringRef value, WKStringRef key)
{
    toProtectedImpl(preferencesRef)->setStringValueForKey(toWTFString(key), toWTFString(value), true);
}

void WKPreferencesResetTestRunnerOverrides(WKPreferencesRef preferencesRef)
{
    // Currently we reset the overrides on the web process when preferencesDidChange() is called. Since WTR preferences
    // are usually always the same (in the UI process), they are not sent to web process, not triggering the reset.
    toProtectedImpl(preferencesRef)->forceUpdate();
}

void WKPreferencesSetJavaScriptEnabled(WKPreferencesRef preferencesRef, bool javaScriptEnabled)
{
    toProtectedImpl(preferencesRef)->setJavaScriptEnabled(javaScriptEnabled);
}

bool WKPreferencesGetJavaScriptEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->javaScriptEnabled();
}

void WKPreferencesSetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef, bool javaScriptMarkupEnabled)
{
    toProtectedImpl(preferencesRef)->setJavaScriptMarkupEnabled(javaScriptMarkupEnabled);
}

bool WKPreferencesGetJavaScriptMarkupEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->javaScriptMarkupEnabled();
}

void WKPreferencesSetLoadsImagesAutomatically(WKPreferencesRef preferencesRef, bool loadsImagesAutomatically)
{
    toProtectedImpl(preferencesRef)->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

bool WKPreferencesGetLoadsImagesAutomatically(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->loadsImagesAutomatically();
}

void WKPreferencesSetLocalStorageEnabled(WKPreferencesRef preferencesRef, bool localStorageEnabled)
{
    toProtectedImpl(preferencesRef)->setLocalStorageEnabled(localStorageEnabled);
}

bool WKPreferencesGetLocalStorageEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->localStorageEnabled();
}

void WKPreferencesSetDatabasesEnabled(WKPreferencesRef preferencesRef, bool databasesEnabled)
{
    toProtectedImpl(preferencesRef)->setDatabasesEnabled(databasesEnabled);
}

bool WKPreferencesGetDatabasesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->databasesEnabled();
}

void WKPreferencesSetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef, bool javaScriptCanOpenWindowsAutomatically)
{
    toProtectedImpl(preferencesRef)->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

bool WKPreferencesGetJavaScriptCanOpenWindowsAutomatically(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->javaScriptCanOpenWindowsAutomatically();
}

void WKPreferencesSetStandardFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setStandardFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyStandardFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->standardFontFamily());
}

void WKPreferencesSetFixedFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setFixedFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFixedFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->fixedFontFamily());
}

void WKPreferencesSetSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->serifFontFamily());
}

void WKPreferencesSetSansSerifFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setSansSerifFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopySansSerifFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->sansSerifFontFamily());
}

void WKPreferencesSetCursiveFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setCursiveFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyCursiveFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->cursiveFontFamily());
}

void WKPreferencesSetFantasyFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setFantasyFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyFantasyFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->fantasyFontFamily());
}

void WKPreferencesSetPictographFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setPictographFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyPictographFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->pictographFontFamily());
}

void WKPreferencesSetMathFontFamily(WKPreferencesRef preferencesRef, WKStringRef family)
{
    toProtectedImpl(preferencesRef)->setMathFontFamily(toWTFString(family));
}

WKStringRef WKPreferencesCopyMathFontFamily(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->mathFontFamily());
}

void WKPreferencesSetDefaultFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toProtectedImpl(preferencesRef)->setDefaultFontSize(size);
}

uint32_t WKPreferencesGetDefaultFontSize(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->defaultFontSize();
}

void WKPreferencesSetDefaultFixedFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toProtectedImpl(preferencesRef)->setDefaultFixedFontSize(size);
}

uint32_t WKPreferencesGetDefaultFixedFontSize(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->defaultFixedFontSize();
}

void WKPreferencesSetMinimumFontSize(WKPreferencesRef preferencesRef, uint32_t size)
{
    toProtectedImpl(preferencesRef)->setMinimumFontSize(size);
}

uint32_t WKPreferencesGetMinimumFontSize(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->minimumFontSize();
}


void WKPreferencesSetCookieEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setCookieEnabled(enabled);
}

bool WKPreferencesGetCookieEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->cookieEnabled();
}

void WKPreferencesSetEditableLinkBehavior(WKPreferencesRef preferencesRef, WKEditableLinkBehavior wkBehavior)
{
    toProtectedImpl(preferencesRef)->setEditableLinkBehavior(static_cast<uint32_t>(toEditableLinkBehavior(wkBehavior)));
}

WKEditableLinkBehavior WKPreferencesGetEditableLinkBehavior(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(toProtectedImpl(preferencesRef)->editableLinkBehavior()));
}

void WKPreferencesSetDefaultTextEncodingName(WKPreferencesRef preferencesRef, WKStringRef name)
{
    toProtectedImpl(preferencesRef)->setDefaultTextEncodingName(toWTFString(name));
}

WKStringRef WKPreferencesCopyDefaultTextEncodingName(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->defaultTextEncodingName());
}

void WKPreferencesSetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setDeveloperExtrasEnabled(enabled);
}

bool WKPreferencesGetDeveloperExtrasEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->developerExtrasEnabled();
}

void WKPreferencesSetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef, WKJavaScriptRuntimeFlagSet javaScriptRuntimeFlagSet)
{
    toProtectedImpl(preferencesRef)->setJavaScriptRuntimeFlags(javaScriptRuntimeFlagSet);
}

WKJavaScriptRuntimeFlagSet WKPreferencesGetJavaScriptRuntimeFlags(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->javaScriptRuntimeFlags();
}

void WKPreferencesSetTextAreasAreResizable(WKPreferencesRef preferencesRef, bool resizable)
{
    toProtectedImpl(preferencesRef)->setTextAreasAreResizable(resizable);
}

bool WKPreferencesGetTextAreasAreResizable(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->textAreasAreResizable();
}

void WKPreferencesSetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setAcceleratedDrawingEnabled(flag);
}

bool WKPreferencesGetAcceleratedDrawingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->acceleratedDrawingEnabled();
}

void WKPreferencesSetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setCanvasUsesAcceleratedDrawing(flag);
}

bool WKPreferencesGetCanvasUsesAcceleratedDrawing(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->canvasUsesAcceleratedDrawing();
}

void WKPreferencesSetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setAcceleratedCompositingEnabled(flag);
}

bool WKPreferencesGetAcceleratedCompositingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->acceleratedCompositingEnabled();
}

void WKPreferencesSetCompositingBordersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setCompositingBordersVisible(flag);
}

bool WKPreferencesGetCompositingBordersVisible(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->compositingBordersVisible();
}

void WKPreferencesSetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setCompositingRepaintCountersVisible(flag);
}

bool WKPreferencesGetCompositingRepaintCountersVisible(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->compositingRepaintCountersVisible();
}

void WKPreferencesSetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setTiledScrollingIndicatorVisible(flag);
}

bool WKPreferencesGetTiledScrollingIndicatorVisible(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->tiledScrollingIndicatorVisible();
}

void WKPreferencesSetWebGLEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setWebGLEnabled(flag);
}

bool WKPreferencesGetWebGLEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webGLEnabled();
}

void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setNeedsSiteSpecificQuirks(flag);
}

bool WKPreferencesGetNeedsSiteSpecificQuirks(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->needsSiteSpecificQuirks();
}

void WKPreferencesSetForceFTPDirectoryListings(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setForceFTPDirectoryListings(flag);
}

bool WKPreferencesGetForceFTPDirectoryListings(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->forceFTPDirectoryListings();
}

void WKPreferencesSetFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef, WKStringRef pathRef)
{
    toProtectedImpl(preferencesRef)->setFTPDirectoryTemplatePath(toWTFString(pathRef));
}

WKStringRef WKPreferencesCopyFTPDirectoryTemplatePath(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->ftpDirectoryTemplatePath());
}

void WKPreferencesSetTabsToLinks(WKPreferencesRef preferencesRef, bool tabsToLinks)
{
    toProtectedImpl(preferencesRef)->setTabsToLinks(tabsToLinks);
}

bool WKPreferencesGetTabsToLinks(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->tabsToLinks();
}

void WKPreferencesSetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAuthorAndUserStylesEnabled(enabled);
}

bool WKPreferencesGetAuthorAndUserStylesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->authorAndUserStylesEnabled();
}

void WKPreferencesSetShouldPrintBackgrounds(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setShouldPrintBackgrounds(flag);
}

bool WKPreferencesGetShouldPrintBackgrounds(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldPrintBackgrounds();
}

void WKPreferencesSetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setDOMTimersThrottlingEnabled(enabled);
}

bool WKPreferencesGetDOMTimersThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->domTimersThrottlingEnabled();
}

void WKPreferencesSetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setWebArchiveDebugModeEnabled(enabled);
}

bool WKPreferencesGetWebArchiveDebugModeEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webArchiveDebugModeEnabled();
}

void WKPreferencesSetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setLocalFileContentSniffingEnabled(enabled);
}

bool WKPreferencesGetLocalFileContentSniffingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->localFileContentSniffingEnabled();
}

void WKPreferencesSetPageCacheEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setUsesBackForwardCache(enabled);
}

bool WKPreferencesGetPageCacheEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->usesBackForwardCache();
}

void WKPreferencesSetDOMPasteAllowed(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setDOMPasteAllowed(enabled);
}

bool WKPreferencesGetDOMPasteAllowed(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->domPasteAllowed();
}

void WKPreferencesSetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setJavaScriptCanAccessClipboard(enabled);
}

bool WKPreferencesGetJavaScriptCanAccessClipboard(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->javaScriptCanAccessClipboard();
}

void WKPreferencesSetFullScreenEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setFullScreenEnabled(enabled);
}

bool WKPreferencesGetFullScreenEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->fullScreenEnabled();
}

void WKPreferencesSetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAsynchronousSpellCheckingEnabled(enabled);
}

bool WKPreferencesGetAsynchronousSpellCheckingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->asynchronousSpellCheckingEnabled();
}

void WKPreferencesSetAVFoundationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAVFoundationEnabled(enabled);
}

bool WKPreferencesGetAVFoundationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->isAVFoundationEnabled();
}

void WKPreferencesSetWebSecurityEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setWebSecurityEnabled(enabled);
}

bool WKPreferencesGetWebSecurityEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webSecurityEnabled();
}

void WKPreferencesSetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    toProtectedImpl(preferencesRef)->setAllowUniversalAccessFromFileURLs(allowed);
}

bool WKPreferencesGetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowUniversalAccessFromFileURLs();
}

void WKPreferencesSetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    toProtectedImpl(preferencesRef)->setAllowFileAccessFromFileURLs(allowed);
}

bool WKPreferencesGetFileAccessFromFileURLsAllowed(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowFileAccessFromFileURLs();
}

void WKPreferencesSetTopNavigationToDataURLsAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    toProtectedImpl(preferencesRef)->setAllowTopNavigationToDataURLs(allowed);
}

bool WKPreferencesGetTopNavigationToDataURLsAllowed(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowTopNavigationToDataURLs();
}

void WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef, bool needsQuirk)
{
    toProtectedImpl(preferencesRef)->setNeedsStorageAccessFromFileURLsQuirk(needsQuirk);
}

bool WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->needsStorageAccessFromFileURLsQuirk();
}

void WKPreferencesSetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setRequiresUserGestureForMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->requiresUserGestureForMediaPlayback();
}

void WKPreferencesSetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setRequiresUserGestureForVideoPlayback(flag);
}

bool WKPreferencesGetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->requiresUserGestureForVideoPlayback();
}

void WKPreferencesSetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setRequiresUserGestureForAudioPlayback(flag);
}

bool WKPreferencesGetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->requiresUserGestureForAudioPlayback();
}

void WKPreferencesSetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setMainContentUserGestureOverrideEnabled(flag);
}

bool WKPreferencesGetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mainContentUserGestureOverrideEnabled();
}

bool WKPreferencesGetVerifyUserGestureInUIProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->verifyWindowOpenUserGestureFromUIProcess();
}

void WKPreferencesSetManagedMediaSourceLowThreshold(WKPreferencesRef preferencesRef, double threshold)
{
    toProtectedImpl(preferencesRef)->setManagedMediaSourceLowThreshold(threshold);
}

double WKPreferencesGetManagedMediaSourceLowThreshold(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->managedMediaSourceLowThreshold();
}

void WKPreferencesSetManagedMediaSourceHighThreshold(WKPreferencesRef preferencesRef, double threshold)
{
    toProtectedImpl(preferencesRef)->setManagedMediaSourceHighThreshold(threshold);
}

double WKPreferencesGetManagedMediaSourceHighThreshold(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->managedMediaSourceHighThreshold();
}

void WKPreferencesSetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setAllowsInlineMediaPlayback(flag);
}

bool WKPreferencesGetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowsInlineMediaPlayback();
}

void WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setInlineMediaPlaybackRequiresPlaysInlineAttribute(flag);
}

bool WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

void WKPreferencesSetBeaconAPIEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setBeaconAPIEnabled(flag);
}

bool WKPreferencesGetBeaconAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->beaconAPIEnabled();
}

void WKPreferencesSetDirectoryUploadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setDirectoryUploadEnabled(flag);
}

bool WKPreferencesGetDirectoryUploadEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->directoryUploadEnabled();
}

void WKPreferencesSetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setMediaControlsScaleWithPageZoom(flag);
}

bool WKPreferencesGetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaControlsScaleWithPageZoom();
}

void WKPreferencesSetWebAuthenticationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setWebAuthenticationEnabled(flag);
}

bool WKPreferencesGetWebAuthenticationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webAuthenticationEnabled();
}

void WKPreferencesSetDigitalCredentialsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setDigitalCredentialsEnabled(flag);
}

bool WKPreferencesGetDigitalCredentialsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->digitalCredentialsEnabled();
}

void WKPreferencesSetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setInvisibleAutoplayNotPermitted(!flag);
}

bool WKPreferencesGetInvisibleMediaAutoplayPermitted(WKPreferencesRef preferencesRef)
{
    return !toProtectedImpl(preferencesRef)->invisibleAutoplayNotPermitted();
}

void WKPreferencesSetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setShowsToolTipOverTruncatedText(flag);
}

bool WKPreferencesGetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->showsToolTipOverTruncatedText();
}

void WKPreferencesSetMockScrollbarsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setMockScrollbarsEnabled(flag);
}

bool WKPreferencesGetMockScrollbarsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mockScrollbarsEnabled();
}

void WKPreferencesSetAttachmentElementEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setAttachmentElementEnabled(flag);
}

bool WKPreferencesGetAttachmentElementEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->attachmentElementEnabled();
}

void WKPreferencesSetWebAudioEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setWebAudioEnabled(enabled);
}

bool WKPreferencesGetWebAudioEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webAudioEnabled();
}

void WKPreferencesSetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setSuppressesIncrementalRendering(enabled);
}

bool WKPreferencesGetSuppressesIncrementalRendering(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->suppressesIncrementalRendering();
}

void WKPreferencesSetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setBackspaceKeyNavigationEnabled(enabled);
}

bool WKPreferencesGetBackspaceKeyNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->backspaceKeyNavigationEnabled();
}

void WKPreferencesSetCaretBrowsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setCaretBrowsingEnabled(enabled);
}

bool WKPreferencesGetCaretBrowsingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->caretBrowsingEnabled();
}

void WKPreferencesSetShouldDisplaySubtitles(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setShouldDisplaySubtitles(enabled);
}

bool WKPreferencesGetShouldDisplaySubtitles(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldDisplaySubtitles();
}

void WKPreferencesSetShouldDisplayCaptions(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setShouldDisplayCaptions(enabled);
}

bool WKPreferencesGetShouldDisplayCaptions(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldDisplayCaptions();
}

void WKPreferencesSetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setShouldDisplayTextDescriptions(enabled);
}

bool WKPreferencesGetShouldDisplayTextDescriptions(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldDisplayTextDescriptions();
}

void WKPreferencesSetNotificationsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setNotificationsEnabled(enabled);
}

bool WKPreferencesGetNotificationsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->notificationsEnabled();
}

void WKPreferencesSetShouldRespectImageOrientation(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setShouldRespectImageOrientation(enabled);
}

bool WKPreferencesGetShouldRespectImageOrientation(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldRespectImageOrientation();
}

void WKPreferencesSetStorageBlockingPolicy(WKPreferencesRef preferencesRef, WKStorageBlockingPolicy policy)
{
    toProtectedImpl(preferencesRef)->setStorageBlockingPolicy(static_cast<uint32_t>(toStorageBlockingPolicy(policy)));
}

WKStorageBlockingPolicy WKPreferencesGetStorageBlockingPolicy(WKPreferencesRef preferencesRef)
{
    return toAPI(static_cast<WebCore::StorageBlockingPolicy>(toProtectedImpl(preferencesRef)->storageBlockingPolicy()));
}

void WKPreferencesSetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setDiagnosticLoggingEnabled(enabled);
}

bool WKPreferencesGetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->diagnosticLoggingEnabled();
}

void WKPreferencesSetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setInteractiveFormValidationEnabled(enabled);
}

bool WKPreferencesGetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->interactiveFormValidationEnabled();
}

void WKPreferencesSetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setScrollingPerformanceTestingEnabled(enabled);
}

bool WKPreferencesGetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->scrollingPerformanceTestingEnabled();
}

void WKPreferencesSetPDFPluginEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setPDFPluginEnabled(enabled);
}

bool WKPreferencesGetPDFPluginEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->pdfPluginEnabled();
}

void WKPreferencesSetEncodingDetectorEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setUsesEncodingDetector(enabled);
}

bool WKPreferencesGetEncodingDetectorEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->usesEncodingDetector();
}

void WKPreferencesSetTextAutosizingEnabled(WKPreferencesRef preferencesRef, bool textAutosizingEnabled)
{
    toProtectedImpl(preferencesRef)->setTextAutosizingEnabled(textAutosizingEnabled);
}

bool WKPreferencesGetTextAutosizingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->textAutosizingEnabled();
}

void WKPreferencesSetTextAutosizingUsesIdempotentMode(WKPreferencesRef preferencesRef, bool textAutosizingUsesIdempotentModeEnabled)
{
    toProtectedImpl(preferencesRef)->setTextAutosizingUsesIdempotentMode(textAutosizingUsesIdempotentModeEnabled);
}

bool WKPreferencesGetTextAutosizingUsesIdempotentMode(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->textAutosizingUsesIdempotentMode();
}

void WKPreferencesSetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAggressiveTileRetentionEnabled(enabled);
}

bool WKPreferencesGetAggressiveTileRetentionEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->aggressiveTileRetentionEnabled();
}

void WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setLogsPageMessagesToSystemConsoleEnabled(enabled);
}

bool WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->logsPageMessagesToSystemConsoleEnabled();
}

void WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

bool WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->pageVisibilityBasedProcessSuppressionEnabled();
}

void WKPreferencesSetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setSmartInsertDeleteEnabled(enabled);
}

bool WKPreferencesGetSmartInsertDeleteEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->smartInsertDeleteEnabled();
}

void WKPreferencesSetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setSelectTrailingWhitespaceEnabled(enabled);
}

bool WKPreferencesGetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->selectTrailingWhitespaceEnabled();
}

void WKPreferencesSetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setShowsURLsInToolTipsEnabled(enabled);
}

bool WKPreferencesGetShowsURLsInToolTipsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->showsURLsInToolTipsEnabled();
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setHiddenPageDOMTimerThrottlingEnabled(enabled);
}

void WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setHiddenPageDOMTimerThrottlingAutoIncreases(enabled);
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->hiddenPageDOMTimerThrottlingEnabled();
}

bool WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->hiddenPageDOMTimerThrottlingAutoIncreases();
}

void WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setHiddenPageCSSAnimationSuspensionEnabled(enabled);
}

bool WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->hiddenPageCSSAnimationSuspensionEnabled();
}

void WKPreferencesSetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef, double timeout)
{
    toProtectedImpl(preferencesRef)->setIncrementalRenderingSuppressionTimeout(timeout);
}

double WKPreferencesGetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->incrementalRenderingSuppressionTimeout();
}

void WKPreferencesSetThreadedScrollingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setThreadedScrollingEnabled(enabled);
}

bool WKPreferencesGetThreadedScrollingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->threadedScrollingEnabled();
}

void WKPreferencesSetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setLegacyLineLayoutVisualCoverageEnabled(flag);
}

bool WKPreferencesGetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->legacyLineLayoutVisualCoverageEnabled();
}

void WKPreferencesSetContentChangeObserverEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setContentChangeObserverEnabled(flag);
}

bool WKPreferencesGetContentChangeObserverEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->contentChangeObserverEnabled();
}

void WKPreferencesSetUseGiantTiles(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setUseGiantTiles(flag);
}

bool WKPreferencesGetUseGiantTiles(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->useGiantTiles();
}

void WKPreferencesSetMediaDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setMediaDevicesEnabled(enabled);
}

bool WKPreferencesGetMediaDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaDevicesEnabled();
}

void WKPreferencesSetPeerConnectionEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setPeerConnectionEnabled(enabled);
}

bool WKPreferencesGetPeerConnectionEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->peerConnectionEnabled();
}

void WKPreferencesSetSpatialNavigationEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setSpatialNavigationEnabled(enabled);
}

bool WKPreferencesGetSpatialNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->spatialNavigationEnabled();
}

void WKPreferencesSetMediaSourceEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setMediaSourceEnabled(enabled);
}

bool WKPreferencesGetMediaSourceEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaSourceEnabled();
}

void WKPreferencesSetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setSourceBufferChangeTypeEnabled(enabled);
}

bool WKPreferencesGetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->sourceBufferChangeTypeEnabled();
}

void WKPreferencesSetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setViewGestureDebuggingEnabled(enabled);
}

bool WKPreferencesGetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->viewGestureDebuggingEnabled();
}

void WKPreferencesSetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef, bool convert)
{
    toProtectedImpl(preferencesRef)->setShouldConvertPositionStyleOnCopy(convert);
}

bool WKPreferencesGetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldConvertPositionStyleOnCopy();
}

void WKPreferencesSetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setTelephoneNumberParsingEnabled(enabled);
}

bool WKPreferencesGetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->telephoneNumberParsingEnabled();
}

void WKPreferencesSetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setEnableInheritURIQueryComponent(enabled);
}

bool WKPreferencesGetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->enableInheritURIQueryComponent();
}

void WKPreferencesSetServiceControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setServiceControlsEnabled(enabled);
}

bool WKPreferencesGetServiceControlsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->serviceControlsEnabled();
}

void WKPreferencesSetImageControlsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setImageControlsEnabled(enabled);
}

bool WKPreferencesGetImageControlsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->imageControlsEnabled();
}

void WKPreferencesSetGamepadsEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setGamepadsEnabled(enabled);
}

bool WKPreferencesGetGamepadsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->gamepadsEnabled();
}

void WKPreferencesSetMinimumZoomFontSize(WKPreferencesRef preferencesRef, double size)
{
    toProtectedImpl(preferencesRef)->setMinimumZoomFontSize(size);
}

double WKPreferencesGetMinimumZoomFontSize(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->minimumZoomFontSize();
}

void WKPreferencesSetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef, WKDebugOverlayRegions visibleRegions)
{
    toProtectedImpl(preferencesRef)->setVisibleDebugOverlayRegions(visibleRegions);
}

WKDebugOverlayRegions WKPreferencesGetVisibleDebugOverlayRegions(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->visibleDebugOverlayRegions();
}

void WKPreferencesSetMetaRefreshEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetMetaRefreshEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->httpEquivEnabled();
}

void WKPreferencesSetHTTPEquivEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setHTTPEquivEnabled(enabled);
}

bool WKPreferencesGetHTTPEquivEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->httpEquivEnabled();
}

void WKPreferencesSetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAllowsAirPlayForMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsAirPlayForMediaPlayback(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowsAirPlayForMediaPlayback();
}

void WKPreferencesSetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef, _WKUserInterfaceDirectionPolicy userInterfaceDirectionPolicy)
{
    toProtectedImpl(preferencesRef)->setUserInterfaceDirectionPolicy(userInterfaceDirectionPolicy);
}

_WKUserInterfaceDirectionPolicy WKPreferencesGetUserInterfaceDirectionPolicy(WKPreferencesRef preferencesRef)
{
    return static_cast<_WKUserInterfaceDirectionPolicy>(toProtectedImpl(preferencesRef)->userInterfaceDirectionPolicy());
}

void WKPreferencesSetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setResourceUsageOverlayVisible(enabled);
}

bool WKPreferencesGetResourceUsageOverlayVisible(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->resourceUsageOverlayVisible();
}

void WKPreferencesSetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setMockCaptureDevicesEnabled(enabled);
}

bool WKPreferencesGetMockCaptureDevicesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mockCaptureDevicesEnabled();
}

void WKPreferencesSetGetUserMediaRequiresFocus(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setGetUserMediaRequiresFocus(enabled);
}

bool WKPreferencesGetGetUserMediaRequiresFocus(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->getUserMediaRequiresFocus();
}

void WKPreferencesSetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setICECandidateFilteringEnabled(enabled);
}

bool WKPreferencesGetICECandidateFilteringEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->iceCandidateFilteringEnabled();
}

void WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

bool WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->enumeratingAllNetworkInterfacesEnabled();
}

void WKPreferencesSetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setMediaCaptureRequiresSecureConnection(enabled);
}

bool WKPreferencesGetMediaCaptureRequiresSecureConnection(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaCaptureRequiresSecureConnection();
}

void WKPreferencesSetInactiveMediaCaptureStreamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef, double interval)
{
    toProtectedImpl(preferencesRef)->setInactiveMediaCaptureStreamRepromptIntervalInMinutes(interval);
}

double WKPreferencesGetInactiveMediaCaptureStreamRepromptIntervalInMinutes(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->inactiveMediaCaptureStreamRepromptIntervalInMinutes();
}

void WKPreferencesSetDataTransferItemsEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setDataTransferItemsEnabled(flag);
}

bool WKPreferencesGetDataTransferItemsEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->dataTransferItemsEnabled();
}

void WKPreferencesSetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setCustomPasteboardDataEnabled(flag);
}

bool WKPreferencesGetCustomPasteboardDataEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->customPasteboardDataEnabled();
}

void WKPreferencesSetWriteRichTextDataWhenCopyingOrDragging(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setWriteRichTextDataWhenCopyingOrDragging(flag);
}

bool WKPreferencesGetWriteRichTextDataWhenCopyingOrDragging(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->writeRichTextDataWhenCopyingOrDragging();
}

void WKPreferencesSetWebShareEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setWebShareEnabled(flag);
}

bool WKPreferencesGetWebShareEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webShareEnabled();
}

void WKPreferencesSetDownloadAttributeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setDownloadAttributeEnabled(flag);
}

bool WKPreferencesGetDownloadAttributeEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->downloadAttributeEnabled();
}

void WKPreferencesSetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setWebRTCPlatformCodecsInGPUProcessEnabled(flag);
}

bool WKPreferencesGetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->webRTCPlatformCodecsInGPUProcessEnabled();
}

WK_EXPORT void WKPreferencesSetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setIsAccessibilityIsolatedTreeEnabled(flag);
}

WK_EXPORT bool WKPreferencesGetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->isAccessibilityIsolatedTreeEnabled();
}

void WKPreferencesSetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setAllowsPictureInPictureMediaPlayback(enabled);
}

bool WKPreferencesGetAllowsPictureInPictureMediaPlayback(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowsPictureInPictureMediaPlayback();
}

WK_EXPORT bool WKPreferencesGetApplePayEnabled(WKPreferencesRef preferencesRef)
{
    return WebKit::toProtectedImpl(preferencesRef)->applePayEnabled();
}

void WKPreferencesSetApplePayEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    WebKit::toProtectedImpl(preferencesRef)->setApplePayEnabled(enabled);
}

bool WKPreferencesGetCSSTransformStyleSeparatedEnabled(WKPreferencesRef preferencesRef)
{
    return WebKit::toProtectedImpl(preferencesRef)->cssTransformStyleSeparatedEnabled();
}

void WKPreferencesSetCSSTransformStyleSeparatedEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    WebKit::toProtectedImpl(preferencesRef)->setCSSTransformStyleSeparatedEnabled(enabled);
}

bool WKPreferencesGetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef)
{
    return WebKit::toProtectedImpl(preferencesRef)->applePayCapabilityDisclosureAllowed();
}

void WKPreferencesSetApplePayCapabilityDisclosureAllowed(WKPreferencesRef preferencesRef, bool allowed)
{
    WebKit::toProtectedImpl(preferencesRef)->setApplePayCapabilityDisclosureAllowed(allowed);
}

void WKPreferencesSetLinkPreloadEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setLinkPreloadEnabled(flag);
}

bool WKPreferencesGetLinkPreloadEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->linkPreloadEnabled();
}

void WKPreferencesSetMediaPreloadingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setMediaPreloadingEnabled(flag);
}

bool WKPreferencesGetMediaPreloadingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaPreloadingEnabled();
}

void WKPreferencesSetExposeSpeakersEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setExposeSpeakersEnabled(flag);
}

bool WKPreferencesGetExposeSpeakersEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->exposeSpeakersEnabled();
}

void WKPreferencesSetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setLargeImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->largeImageAsyncDecodingEnabled();
}

void WKPreferencesSetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setAnimatedImageAsyncDecodingEnabled(flag);
}

bool WKPreferencesGetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->animatedImageAsyncDecodingEnabled();
}

void WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(flag);
}

bool WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

void WKPreferencesSetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setMediaUserGestureInheritsFromDocument(flag);
}

bool WKPreferencesGetMediaUserGestureInheritsFromDocument(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaUserGestureInheritsFromDocument();
}

void WKPreferencesSetMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef, WKStringRef codecs)
{
    toProtectedImpl(preferencesRef)->setMediaContentTypesRequiringHardwareSupport(toWTFString(codecs));
}

WKStringRef WKPreferencesCopyMediaContentTypesRequiringHardwareSupport(WKPreferencesRef preferencesRef)
{
    return toCopiedAPI(toProtectedImpl(preferencesRef)->mediaContentTypesRequiringHardwareSupport());
}

bool WKPreferencesGetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->legacyEncryptedMediaAPIEnabled();
}

void WKPreferencesSetLegacyEncryptedMediaAPIEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    return toProtectedImpl(preferencesRef)->setLegacyEncryptedMediaAPIEnabled(enabled);
}

bool WKPreferencesGetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->allowMediaContentTypesRequiringHardwareSupportAsFallback();
}

void WKPreferencesSetAllowMediaContentTypesRequiringHardwareSupportAsFallback(WKPreferencesRef preferencesRef, bool allow)
{
    return toProtectedImpl(preferencesRef)->setAllowMediaContentTypesRequiringHardwareSupportAsFallback(allow);
}

void WKPreferencesSetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setShouldAllowUserInstalledFonts(flag);
}

bool WKPreferencesGetShouldAllowUserInstalledFonts(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->shouldAllowUserInstalledFonts();
}

void WKPreferencesSetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setMediaCapabilitiesEnabled(enabled);
}

bool WKPreferencesGetMediaCapabilitiesEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->mediaCapabilitiesEnabled();
}

void WKPreferencesSetColorFilterEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setColorFilterEnabled(flag);
}

bool WKPreferencesGetColorFilterEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->colorFilterEnabled();
}

void WKPreferencesSetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setProcessSwapOnCrossSiteNavigationEnabled(flag);
}

bool WKPreferencesGetProcessSwapOnNavigationEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->processSwapOnCrossSiteNavigationEnabled();
}

void WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setPunchOutWhiteBackgroundsInDarkMode(flag);
}

bool WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->punchOutWhiteBackgroundsInDarkMode();
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
    toProtectedImpl(preferencesRef)->setCaptureAudioInGPUProcessEnabled(flag);
}

bool WKPreferencesGetCaptureAudioInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->captureAudioInGPUProcessEnabled();
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
    toProtectedImpl(preferencesRef)->setCaptureVideoInGPUProcessEnabled(flag);
}

bool WKPreferencesGetCaptureVideoInGPUProcessEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->captureVideoInGPUProcessEnabled();
}

void WKPreferencesSetVP9DecoderEnabled(WKPreferencesRef preferencesRef, bool flag)
{
    toProtectedImpl(preferencesRef)->setVP9DecoderEnabled(flag);
}

bool WKPreferencesGetVP9DecoderEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->vp9DecoderEnabled();
}

bool WKPreferencesGetRemotePlaybackEnabled(WKPreferencesRef preferencesRef)
{
    return WebKit::toProtectedImpl(preferencesRef)->remotePlaybackEnabled();
}

void WKPreferencesSetRemotePlaybackEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    WebKit::toProtectedImpl(preferencesRef)->setRemotePlaybackEnabled(enabled);
}

bool WKPreferencesGetShouldUseServiceWorkerShortTimeout(WKPreferencesRef preferencesRef)
{
    return WebKit::toProtectedImpl(preferencesRef)->shouldUseServiceWorkerShortTimeout();
}

void WKPreferencesSetShouldUseServiceWorkerShortTimeout(WKPreferencesRef preferencesRef, bool enabled)
{
    WebKit::toProtectedImpl(preferencesRef)->setShouldUseServiceWorkerShortTimeout(enabled);
}

void WKPreferencesSetRequestVideoFrameCallbackEnabled(WKPreferencesRef preferencesRef, bool enabled)
{
    toProtectedImpl(preferencesRef)->setRequestVideoFrameCallbackEnabled(enabled);
}

bool WKPreferencesGetRequestVideoFrameCallbackEnabled(WKPreferencesRef preferencesRef)
{
    return toProtectedImpl(preferencesRef)->requestVideoFrameCallbackEnabled();
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
