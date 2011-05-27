/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebSystemInterface.h"

// Needed for builds not using PCH to expose BUILDING_ macros, see bug 32753.
#include <wtf/Platform.h>

#import <WebCore/WebCoreSystemInterface.h>
#import <WebKitSystemInterface.h>

#define INIT(function) wk##function = WK##function

void InitWebCoreSystemInterface(void)
{
    static bool didInit;
    if (didInit)
        return;

    INIT(AdvanceDefaultButtonPulseAnimation);
    INIT(CGContextGetShouldSmoothFonts);
    INIT(CopyCFLocalizationPreferredName);
    INIT(CopyCONNECTProxyResponse);
    INIT(CopyNSURLResponseStatusLine);
    INIT(CreateCustomCFReadStream);
    INIT(CreateNSURLConnectionDelegateProxy);
    INIT(DrawCapsLockIndicator);
    INIT(DrawBezeledTextArea);
    INIT(DrawBezeledTextFieldCell);
    INIT(DrawFocusRing);
    INIT(DrawMediaUIPart);
    INIT(DrawMediaSliderTrack);
    INIT(DrawTextFieldCellFocusRing);
    INIT(GetExtensionsForMIMEType);
    INIT(GetFontInLanguageForCharacter);
    INIT(GetFontInLanguageForRange);
    INIT(GetGlyphTransformedAdvances);
    INIT(GetHTTPPipeliningPriority);
    INIT(GetMIMETypeForExtension);
    INIT(GetNSURLResponseLastModifiedDate);
    INIT(SignedPublicKeyAndChallengeString);
    INIT(GetPreferredExtensionForMIMEType);
    INIT(GetWheelEventDeltas);
    INIT(HitTestMediaUIPart);
    INIT(InitializeMaximumHTTPConnectionCountPerHost);
    INIT(MeasureMediaUIPart);
    INIT(MediaControllerThemeAvailable);
    INIT(PopupMenu);
    INIT(SetCGFontRenderingMode);
    INIT(SetCONNECTProxyAuthorizationForStream);
    INIT(SetCONNECTProxyForStream);
    INIT(SetCookieStoragePrivateBrowsingEnabled);
    INIT(SetDragImage);
    INIT(SetHTTPPipeliningMaximumPriority);
    INIT(SetHTTPPipeliningPriority);
    INIT(SetHTTPPipeliningMinimumFastLanePriority);
    INIT(SetNSURLConnectionDefersCallbacks);
    INIT(SetNSURLRequestShouldContentSniff);
    INIT(SetPatternBaseCTM);
    INIT(SetPatternPhaseInUserSpace);
    INIT(GetUserToBaseCTM);
    INIT(SetUpFontCache);
    INIT(SignalCFReadStreamEnd);
    INIT(SignalCFReadStreamError);
    INIT(SignalCFReadStreamHasBytes);
    INIT(QTIncludeOnlyModernMediaFileTypes);
    INIT(QTMovieDataRate);
    INIT(QTMovieDisableComponent);
    INIT(QTMovieMaxTimeLoaded);
    INIT(QTMovieMaxTimeLoadedChangeNotification);
    INIT(QTMovieMaxTimeSeekable);
    INIT(QTMovieGetType);
    INIT(QTMovieHasClosedCaptions);
    INIT(QTMovieResolvedURL);
    INIT(QTMovieSetShowClosedCaptions);
    INIT(QTMovieSelectPreferredAlternates);
    INIT(QTMovieViewSetDrawSynchronously);
    INIT(QTGetSitesInMediaDownloadCache);
    INIT(QTClearMediaDownloadCacheForSite);
    INIT(QTClearMediaDownloadCache);

    INIT(GetGlyphsForCharacters);

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD)
    INIT(GetHyphenationLocationBeforeIndex);
    INIT(GetNSEventMomentumPhase);
#endif

    INIT(CreateCTLineWithUniCharProvider);

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    INIT(IOSurfaceContextCreate);
    INIT(IOSurfaceContextCreateImage);
    INIT(CreateCTTypesetterWithUniCharProviderAndOptions);
    INIT(MakeScrollbarPainter);
    INIT(ScrollbarPainterSetDelegate);
    INIT(ScrollbarPainterPaint);
    INIT(ScrollbarPainterForceFlashScrollers);
    INIT(ScrollbarThickness);
    INIT(ScrollbarMinimumThumbLength);
    INIT(ScrollbarMinimumTotalLengthNeededForThumb);
    INIT(ScrollbarPainterKnobAlpha);
    INIT(SetScrollbarPainterKnobAlpha);
    INIT(ScrollbarPainterTrackAlpha);
    INIT(SetScrollbarPainterTrackAlpha);
    INIT(ScrollbarPainterIsHorizontal);
    INIT(ScrollbarPainterKnobRect);
    INIT(ScrollbarPainterSetOverlayState);
    INIT(SetScrollbarPainterKnobStyle);
    INIT(MakeScrollbarPainterController);
    INIT(MakeScrollbarReplacementPainter);
    INIT(SetPainterForPainterController);
    INIT(VerticalScrollbarPainterForController);
    INIT(HorizontalScrollbarPainterForController);
    INIT(ScrollbarPainterControllerStyle);
    INIT(SetScrollbarPainterControllerStyle);
    INIT(ContentAreaScrolled);
    INIT(ContentAreaWillPaint);
    INIT(MouseEnteredContentArea);
    INIT(MouseExitedContentArea);
    INIT(MouseMovedInContentArea);
    INIT(WillStartLiveResize);
    INIT(ContentAreaResized);
    INIT(WillEndLiveResize);
    INIT(ContentAreaDidShow);
    INIT(ContentAreaDidHide);
    INIT(DidBeginScrollGesture);
    INIT(DidEndScrollGesture);
    INIT(ScrollbarPainterUsesOverlayScrollers);
    INIT(ExecutableWasLinkedOnOrBeforeSnowLeopard);
    INIT(CopyDefaultSearchProviderDisplayName);
    INIT(AVAssetResolvedURL);
#endif

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
    INIT(CreatePrivateStorageSession);
    INIT(CopyRequestWithStorageSession);
    INIT(CopyHTTPCookieStorage);
    INIT(GetHTTPCookieAcceptPolicy);
    INIT(HTTPCookiesForURL);
    INIT(SetHTTPCookiesForURL);
    INIT(DeleteHTTPCookie);

    INIT(GetCFURLResponseMIMEType);
    INIT(GetCFURLResponseURL);
    INIT(GetCFURLResponseHTTPResponse);
    INIT(CopyCFURLResponseSuggestedFilename);
    INIT(SetCFURLResponseMIMEType);

    didInit = true;
}
