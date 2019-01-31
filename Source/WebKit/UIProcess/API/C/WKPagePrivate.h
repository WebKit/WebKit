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

#ifndef WKPagePrivate_h
#define WKPagePrivate_h

#include <WebKit/WKBase.h>
#include <WebKit/WKPage.h>

#if defined(WIN32) || defined(_WIN32)
typedef int WKProcessID;
#else
#include <unistd.h>
typedef pid_t WKProcessID;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WKPageRenderTreeExternalRepresentationFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageRenderTreeExternalRepresentation(WKPageRef page, void *context, WKPageRenderTreeExternalRepresentationFunction function);

enum {
    kWKDebugFlashViewUpdates = 1 << 0,
    kWKDebugFlashBackingStoreUpdates = 1 << 1
};
typedef unsigned WKPageDebugPaintFlags;

WK_EXPORT WKStringRef WKPageCopyStandardUserAgentWithApplicationName(WKStringRef);

enum {
    kWKPaginationModeUnpaginated,
    kWKPaginationModeLeftToRight,
    kWKPaginationModeRightToLeft,
    kWKPaginationModeTopToBottom,
    kWKPaginationModeBottomToTop,
};
typedef uint32_t WKPaginationMode;

WK_EXPORT void WKPageSetPaginationMode(WKPageRef page, WKPaginationMode paginationMode);
WK_EXPORT WKPaginationMode WKPageGetPaginationMode(WKPageRef page);
WK_EXPORT void WKPageSetPaginationBehavesLikeColumns(WKPageRef page, bool behavesLikeColumns);
WK_EXPORT bool WKPageGetPaginationBehavesLikeColumns(WKPageRef page);
WK_EXPORT void WKPageSetPageLength(WKPageRef page, double pageLength);
WK_EXPORT double WKPageGetPageLength(WKPageRef page);
WK_EXPORT void WKPageSetGapBetweenPages(WKPageRef page, double gap);
WK_EXPORT double WKPageGetGapBetweenPages(WKPageRef page);
WK_EXPORT void WKPageSetPaginationLineGridEnabled(WKPageRef page, bool lineGridEnabled);
WK_EXPORT bool WKPageGetPaginationLineGridEnabled(WKPageRef page);

WK_EXPORT unsigned WKPageGetPageCount(WKPageRef page);

struct WKPrintInfo {
    float pageSetupScaleFactor;
    float availablePaperWidth;
    float availablePaperHeight;
};
typedef struct WKPrintInfo WKPrintInfo;

typedef void (*WKPageComputePagesForPrintingFunction)(WKRect* pageRects, uint32_t pageCount, double resultPageScaleFactor, WKErrorRef error, void* functionContext);
WK_EXPORT void WKPageComputePagesForPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo, WKPageComputePagesForPrintingFunction, void* context);

typedef void (*WKPageDrawToPDFFunction)(WKDataRef data, WKErrorRef error, void* functionContext);
WK_EXPORT void WKPageBeginPrinting(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo);
WK_EXPORT void WKPageDrawPagesToPDF(WKPageRef page, WKFrameRef frame, WKPrintInfo printInfo, uint32_t first, uint32_t count, WKPageDrawToPDFFunction callback, void* context);
WK_EXPORT void WKPageEndPrinting(WKPageRef page);

WK_EXPORT bool WKPageGetIsControlledByAutomation(WKPageRef page);
WK_EXPORT void WKPageSetControlledByAutomation(WKPageRef page, bool controlled);

WK_EXPORT bool WKPageGetAllowsRemoteInspection(WKPageRef page);
WK_EXPORT void WKPageSetAllowsRemoteInspection(WKPageRef page, bool allow);

WK_EXPORT void WKPageSetMediaVolume(WKPageRef page, float volume);
WK_EXPORT void WKPageSetMayStartMediaWhenInWindow(WKPageRef page, bool mayStartMedia);

typedef void (*WKPageGetBytecodeProfileFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageGetBytecodeProfile(WKPageRef page, void* context, WKPageGetBytecodeProfileFunction function);

typedef void (*WKPageGetSamplingProfilerOutputFunction)(WKStringRef, WKErrorRef, void*);
WK_EXPORT void WKPageGetSamplingProfilerOutput(WKPageRef page, void* context, WKPageGetSamplingProfilerOutputFunction function);

WK_EXPORT WKArrayRef WKPageCopyRelatedPages(WKPageRef page);

WK_EXPORT WKFrameRef WKPageLookUpFrameFromHandle(WKPageRef page, WKFrameHandleRef handle);

enum {
    kWKScrollPinningBehaviorDoNotPin,
    kWKScrollPinningBehaviorPinToTop,
    kWKScrollPinningBehaviorPinToBottom
};
typedef uint32_t WKScrollPinningBehavior;

WK_EXPORT WKScrollPinningBehavior WKPageGetScrollPinningBehavior(WKPageRef page);
WK_EXPORT void WKPageSetScrollPinningBehavior(WKPageRef page, WKScrollPinningBehavior pinning);

WK_EXPORT bool WKPageGetAddsVisitedLinks(WKPageRef page);
WK_EXPORT void WKPageSetAddsVisitedLinks(WKPageRef page, bool visitedLinks);

WK_EXPORT bool WKPageIsPlayingAudio(WKPageRef page);

enum {
    kWKMediaNoneMuted = 0,
    kWKMediaAudioMuted = 1 << 0,
    kWKMediaCaptureDevicesMuted = 1 << 1,
};
typedef uint32_t WKMediaMutedState;
WK_EXPORT void WKPageSetMuted(WKPageRef page, WKMediaMutedState muted);

WK_EXPORT void WKPageClearUserMediaState(WKPageRef page);
WK_EXPORT void WKPageSetMediaCaptureEnabled(WKPageRef page, bool enabled);
WK_EXPORT bool WKPageGetMediaCaptureEnabled(WKPageRef page);

WK_EXPORT void WKPageDidAllowPointerLock(WKPageRef page);
WK_EXPORT void WKPageDidDenyPointerLock(WKPageRef page);

enum {
    kWKMediaIsNotPlaying = 0,
    kWKMediaIsPlayingAudio = 1 << 0,
    kWKMediaIsPlayingVideo = 1 << 1,
    kWKMediaHasActiveAudioCaptureDevice = 1 << 2,
    kWKMediaHasActiveVideoCaptureDevice = 1 << 3,
    kWKMediaHasMutedAudioCaptureDevice = 1 << 4,
    kWKMediaHasMutedVideoCaptureDevice = 1 << 5,
    kWKMediaHasActiveDisplayCaptureDevice = 1 << 6,
    kWKMediaHasMutedDisplayCaptureDevice = 1 << 7,
};
typedef uint32_t WKMediaState;


WK_EXPORT WKMediaState WKPageGetMediaState(WKPageRef page);

enum {
    kWKMediaEventTypePlayPause,
    kWKMediaEventTypeTrackNext,
    kWKMediaEventTypeTrackPrevious
};
typedef uint32_t WKMediaEventType;

WK_EXPORT bool WKPageHasMediaSessionWithActiveMediaElements(WKPageRef page);
WK_EXPORT void WKPageHandleMediaEvent(WKPageRef page, WKMediaEventType event);

WK_EXPORT void WKPageLoadURLWithShouldOpenExternalURLsPolicy(WKPageRef page, WKURLRef url, bool shouldOpenExternalURLs);

typedef void (*WKPagePostPresentationUpdateFunction)(WKErrorRef, void*);
WK_EXPORT void WKPageCallAfterNextPresentationUpdate(WKPageRef page, void* context, WKPagePostPresentationUpdateFunction function);

WK_EXPORT bool WKPageGetResourceCachingDisabled(WKPageRef page);
WK_EXPORT void WKPageSetResourceCachingDisabled(WKPageRef page, bool disabled);

WK_EXPORT void WKPageRestoreFromSessionStateWithoutNavigation(WKPageRef page, WKTypeRef sessionState);

WK_EXPORT void WKPageSetIgnoresViewportScaleLimits(WKPageRef page, bool ignoresViewportScaleLimits);

WK_EXPORT WKProcessID WKPageGetProcessIdentifier(WKPageRef page);

#ifdef __BLOCKS__
typedef void (^WKPageGetApplicationManifestBlock)(void);
WK_EXPORT void WKPageGetApplicationManifest_b(WKPageRef page, WKPageGetApplicationManifestBlock block);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WKPagePrivate_h */
