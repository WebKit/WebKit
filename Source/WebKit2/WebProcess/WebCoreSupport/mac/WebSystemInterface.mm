/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebSystemInterface.h"

#import <WebCore/WebCoreSystemInterface.h>
#import <WebKitSystemInterface.h>

#define INIT(function) wk##function = WK##function

void InitWebCoreSystemInterface(void)
{
    static dispatch_once_t initOnce;
    
    dispatch_once(&initOnce, ^{
#if !PLATFORM(IOS)
        INIT(AdvanceDefaultButtonPulseAnimation);
#endif
        INIT(CALayerEnumerateRectsBeingDrawnWithBlock);
        INIT(CGPatternCreateWithImageAndTransform);
#if !PLATFORM(IOS)
        INIT(CGContextDrawsWithCorrectShadowOffsets);
#endif
        INIT(CopyCONNECTProxyResponse);
#if !PLATFORM(IOS)
        INIT(DrawBezeledTextArea);
        INIT(DrawFocusRing);
        INIT(DrawFocusRingAtTime);
        INIT(DrawCellFocusRingWithFrameAtTime);
        INIT(DrawMediaSliderTrack);
        INIT(DrawMediaUIPart);
        INIT(SignedPublicKeyAndChallengeString);
#endif
        INIT(GetWebDefaultCFStringEncoding);
        INIT(CGContextIsPDFContext);
#if !PLATFORM(IOS)
        INIT(GetWheelEventDeltas);
        INIT(GetNSEventKeyChar);
        INIT(HitTestMediaUIPart);
        INIT(MeasureMediaUIPart);
        INIT(QTIncludeOnlyModernMediaFileTypes);
        INIT(QTMovieDisableComponent);
        INIT(QTMovieGetType);
        INIT(QTMovieHasClosedCaptions);
        INIT(QTMovieMaxTimeLoaded);
        INIT(QTMovieMaxTimeLoadedChangeNotification);
        INIT(QTMovieResolvedURL);
        INIT(QTMovieSelectPreferredAlternates);
        INIT(QTMovieSetShowClosedCaptions);
        INIT(QTGetSitesInMediaDownloadCache);
        INIT(QTClearMediaDownloadCacheForSite);
        INIT(QTClearMediaDownloadCache);
#endif
        INIT(SetCONNECTProxyAuthorizationForStream);
        INIT(SetCONNECTProxyForStream);
#if !PLATFORM(IOS)
        INIT(SetDragImage);
#endif
        INIT(CreatePrivateStorageSession);
        INIT(CopyRequestWithStorageSession);
        INIT(GetHTTPCookieAcceptPolicy);
        INIT(HTTPCookies);
        INIT(HTTPCookiesForURL);
        INIT(SetHTTPCookiesForURL);
        INIT(DeleteAllHTTPCookies);
        INIT(DeleteHTTPCookie);

#if !PLATFORM(IOS)
        INIT(SetMetadataURL);
        INIT(ExecutableWasLinkedOnOrBeforeSnowLeopard);
        INIT(CopyDefaultSearchProviderDisplayName);
        INIT(Cursor);
        INIT(WindowSetScaledFrame);
        INIT(WindowSetAlpha);
        INIT(SpeechSynthesisGetVoiceIdentifiers);
        INIT(SpeechSynthesisGetDefaultVoiceIdentifierForLocale);
        INIT(GetAXTextMarkerTypeID);
        INIT(GetAXTextMarkerRangeTypeID);
        INIT(CreateAXTextMarker);
        INIT(GetBytesFromAXTextMarker);
        INIT(CreateAXTextMarkerRange);
        INIT(CopyAXTextMarkerRangeStart);
        INIT(CopyAXTextMarkerRangeEnd);
        INIT(AccessibilityHandleFocusChanged);
        INIT(CreateAXUIElementRef);
        INIT(UnregisterUniqueIdForElement);
        INIT(NSElasticDeltaForTimeDelta);
        INIT(NSElasticDeltaForReboundDelta);
        INIT(NSReboundDeltaForElasticDelta);
#endif

#if ENABLE(PUBLIC_SUFFIX_LIST)
        INIT(IsPublicSuffix);
#endif

#if ENABLE(CACHE_PARTITIONING)
        INIT(CachePartitionKey);
#endif

        INIT(ExernalDeviceTypeForPlayer);
        INIT(ExernalDeviceDisplayNameForPlayer);

        INIT(QueryDecoderAvailability);
    });
}
