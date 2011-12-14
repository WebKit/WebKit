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

#ifndef WKPreferencesPrivate_h
#define WKPreferencesPrivate_h

#include <WebKit2/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

enum WKFontSmoothingLevel {
    kWKFontSmoothingLevelNoSubpixelAntiAliasing = 0,
    kWKFontSmoothingLevelLight = 1,
    kWKFontSmoothingLevelMedium = 2,
    kWKFontSmoothingLevelStrong = 3,
#if defined(WIN32) || defined(_WIN32)
    kWKFontSmoothingLevelWindows = 4,
#endif
};
typedef enum WKFontSmoothingLevel WKFontSmoothingLevel;

enum WKEditableLinkBehavior {
    kWKEditableLinkBehaviorDefault,
    kWKEditableLinkBehaviorAlwaysLive,
    kWKEditableLinkBehaviorOnlyLiveWithShiftKey,
    kWKEditableLinkBehaviorLiveWhenNotFocused,
    kWKEditableLinkBehaviorNeverLive
};
typedef enum WKEditableLinkBehavior WKEditableLinkBehavior;

// Defaults to kWKFontSmoothingLevelWindows on Windows, kWKFontSmoothingLevelMedium on other platforms.
WK_EXPORT void WKPreferencesSetFontSmoothingLevel(WKPreferencesRef, WKFontSmoothingLevel);
WK_EXPORT WKFontSmoothingLevel WKPreferencesGetFontSmoothingLevel(WKPreferencesRef);

// Defaults to EditableLinkNeverLive.
WK_EXPORT void WKPreferencesSetEditableLinkBehavior(WKPreferencesRef preferencesRef, WKEditableLinkBehavior);
WK_EXPORT WKEditableLinkBehavior WKPreferencesGetEditableLinkBehavior(WKPreferencesRef preferencesRef);   
    
// Defaults to false.
WK_EXPORT void WKPreferencesSetAcceleratedDrawingEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetAcceleratedDrawingEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetCanvasUsesAcceleratedDrawing(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCanvasUsesAcceleratedDrawing(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetAcceleratedCompositingEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetAcceleratedCompositingEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetCompositingBordersVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCompositingBordersVisible(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetCompositingRepaintCountersVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCompositingRepaintCountersVisible(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetWebGLEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetWebGLEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetNeedsSiteSpecificQuirks(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetForceFTPDirectoryListings(WKPreferencesRef preferences, bool force);
WK_EXPORT bool WKPreferencesGetForceFTPDirectoryListings(WKPreferencesRef preferences);

// Defaults to the empty string.
WK_EXPORT void WKPreferencesSetFTPDirectoryTemplatePath(WKPreferencesRef preferences, WKStringRef path);
WK_EXPORT WKStringRef WKPreferencesCopyFTPDirectoryTemplatePath(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetWebArchiveDebugModeEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetWebArchiveDebugModeEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetLocalFileContentSniffingEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetLocalFileContentSniffingEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetPageCacheEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPageCacheEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetPaginateDuringLayoutEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPaginateDuringLayoutEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetDOMPasteAllowed(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetDOMPasteAllowed(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetWebSecurityEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetWebSecurityEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferences, bool allowed);
WK_EXPORT bool WKPreferencesGetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetFileAccessFromFileURLsAllowed(WKPreferencesRef preferences, bool allowed);
WK_EXPORT bool WKPreferencesGetFileAccessFromFileURLsAllowed(WKPreferencesRef preferences);

#ifdef __cplusplus
}
#endif

#endif /* WKPreferencesPrivate_h */

