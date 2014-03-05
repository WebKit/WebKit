/*
 * Copyright 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#if !PLATFORM(IOS)
    INIT(AdvanceDefaultButtonPulseAnimation);
#endif
    INIT(CALayerEnumerateRectsBeingDrawnWithBlock);
#if !PLATFORM(IOS)
    INIT(CGContextGetShouldSmoothFonts);
#endif
    INIT(CGPatternCreateWithImageAndTransform);
    INIT(CGContextResetClip);
#if !PLATFORM(IOS)
    INIT(CGContextDrawsWithCorrectShadowOffsets);
#endif
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    INIT(CTFontTransformGlyphs);
#endif
    INIT(CopyCFLocalizationPreferredName);
    INIT(CopyCONNECTProxyResponse);
    INIT(CopyNSURLResponseStatusLine);
    INIT(CopyNSURLResponseCertificateChain);
    INIT(CreateCustomCFReadStream);
#if !PLATFORM(IOS)
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
#endif
    INIT(GetHTTPRequestPriority);
    INIT(GetMIMETypeForExtension);
    INIT(GetNSURLResponseLastModifiedDate);
#if !PLATFORM(IOS)
    INIT(SignedPublicKeyAndChallengeString);
    INIT(GetPreferredExtensionForMIMEType);
    INIT(GetWheelEventDeltas);
    INIT(GetNSEventKeyChar);
    INIT(HitTestMediaUIPart);
#endif
    INIT(InitializeMaximumHTTPConnectionCountPerHost);
#if !PLATFORM(IOS)
    INIT(MeasureMediaUIPart);
    INIT(CreateMediaUIBackgroundView);
    INIT(CreateMediaUIControl);
    INIT(WindowSetAlpha);
    INIT(WindowSetScaledFrame);
    INIT(PopupMenu);
    INIT(SetCGFontRenderingMode);
#endif
    INIT(SetBaseCTM);
    INIT(SetCONNECTProxyAuthorizationForStream);
    INIT(SetCONNECTProxyForStream);
#if !PLATFORM(IOS)
    INIT(SetDragImage);
#endif
    INIT(SetHTTPRequestMaximumPriority);
    INIT(SetHTTPRequestPriority);
    INIT(SetHTTPRequestMinimumFastLanePriority);
    INIT(HTTPRequestEnablePipelining);
    INIT(SetNSURLConnectionDefersCallbacks);
    INIT(SetNSURLRequestShouldContentSniff);
    INIT(SetPatternPhaseInUserSpace);
    INIT(CGContextIsPDFContext);
    INIT(GetUserToBaseCTM);
    INIT(SetUpFontCache);
    INIT(SignalCFReadStreamEnd);
    INIT(SignalCFReadStreamError);
    INIT(SignalCFReadStreamHasBytes);
#if ENABLE(VIDEO) && !PLATFORM(IOS)
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
#endif

#if !PLATFORM(IOS)
    INIT(GetGlyphsForCharacters);
#endif
    INIT(GetVerticalGlyphsForCharacters);
#if PLATFORM(IOS)
    INIT(ExecutableWasLinkedOnOrAfterIOSVersion);
    INIT(GetDeviceClass);
    INIT(GetViewportScreenSize);
    INIT(GetScreenScaleFactor);
    INIT(IsGB18030ComplianceRequired);
#endif

    INIT(CreateCTLineWithUniCharProvider);

#if !PLATFORM(IOS_SIMULATOR)
    INIT(IOSurfaceContextCreate);
    INIT(IOSurfaceContextCreateImage);
#endif
    INIT(CreateCTTypesetterWithUniCharProviderAndOptions);
    INIT(CTRunGetInitialAdvance);
#if PLATFORM(MAC) || PLATFORM(IOS_SIMULATOR)
    INIT(SetCrashReportApplicationSpecificInformation);
#endif
#if !PLATFORM(IOS)
    INIT(RecommendedScrollerStyle);
    INIT(ExecutableWasLinkedOnOrBeforeSnowLeopard);
    INIT(CopyDefaultSearchProviderDisplayName);
    INIT(AVAssetResolvedURL);
    INIT(Cursor);
#endif

#if USE(CFNETWORK)
    INIT(GetDefaultHTTPCookieStorage);
    INIT(CopyCredentialFromCFPersistentStorage);
    INIT(SetCFURLRequestShouldContentSniff);
    INIT(CFURLRequestCopyHTTPRequestBodyParts);
    INIT(CFURLRequestSetHTTPRequestBodyParts);
    INIT(SetRequestStorageSession);
#endif

#if !PLATFORM(IOS)
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
#endif
    INIT(CreatePrivateStorageSession);
    INIT(CopyRequestWithStorageSession);
    INIT(CopyHTTPCookieStorage);
    INIT(GetHTTPCookieAcceptPolicy);
    INIT(HTTPCookies);
    INIT(HTTPCookiesForURL);
    INIT(SetHTTPCookiesForURL);
    INIT(DeleteAllHTTPCookies);
    INIT(DeleteHTTPCookie);

    INIT(GetCFURLResponseMIMEType);
    INIT(GetCFURLResponseURL);
    INIT(GetCFURLResponseHTTPResponse);
    INIT(CopyCFURLResponseSuggestedFilename);
    INIT(SetCFURLResponseMIMEType);

#if !PLATFORM(IOS)
    INIT(SetMetadataURL);
#endif

    INIT(DestroyRenderingResources);

#if !PLATFORM(IOS)
    INIT(CreateVMPressureDispatchOnMainQueue);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    INIT(CreateMemoryStatusPressureCriticalDispatchOnMainQueue);
#endif

    INIT(ExecutableWasLinkedOnOrBeforeLion);
#endif

    INIT(CGPathAddRoundedRect);

    INIT(CFURLRequestAllowAllPostCaching);

#if PLATFORM(IOS)
    INIT(GetUserAgent);
    INIT(GetDeviceName);
    INIT(GetOSNameForUserAgent);
    INIT(GetPlatformNameForNavigator);
    INIT(GetVendorNameForNavigator);
#endif

#if !PLATFORM(IOS)
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

    didInit = true;
}
