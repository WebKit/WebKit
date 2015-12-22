/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#pragma GCC visibility push(default)
#import "WebCoreSystemInterface.h"
#pragma GCC visibility pop

#import <Foundation/Foundation.h>

void (*wkAdvanceDefaultButtonPulseAnimation)(NSButtonCell *);
void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef context, void (^block)(CGRect rect));
bool (*wkCGContextDrawsWithCorrectShadowOffsets)(CGContextRef);
CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
CFStringEncoding (*wkGetWebDefaultCFStringEncoding)(void);
void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
void (*wkDrawFocusRing)(CGContextRef, CGColorRef, int);
bool (*wkDrawFocusRingAtTime)(CGContextRef, NSTimeInterval);
bool (*wkDrawCellFocusRingWithFrameAtTime)(NSCell*, NSRect, NSView*, NSTimeInterval);
void (*wkDrawMediaSliderTrack)(CGContextRef context, CGRect rect, float timeLoaded, float currentTime,
    float duration, unsigned state);
BOOL (*wkHitTestMediaUIPart)(int part, CGRect bounds, CGPoint point);
void (*wkDrawMediaUIPart)(int part, CGContextRef context, CGRect rect, unsigned state);
void (*wkMeasureMediaUIPart)(int part, CGRect *bounds, CGSize *naturalSize);
NSView *(*wkCreateMediaUIBackgroundView)(void);
NSControl *(*wkCreateMediaUIControl)(int);
void (*wkWindowSetAlpha)(NSWindow *, float);
void (*wkWindowSetScaledFrame)(NSWindow *, NSRect, NSRect);
CFStringRef (*wkSignedPublicKeyAndChallengeString)(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
NSTimeInterval (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
void (*wkGetWheelEventDeltas)(NSEvent*, float* deltaX, float* deltaY, BOOL* continuous);
UInt8 (*wkGetNSEventKeyChar)(NSEvent *);
void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*, NSControlSize controlSize, bool usesCustomAppearance);
unsigned (*wkQTIncludeOnlyModernMediaFileTypes)(void);
void (*wkQTMovieDisableComponent)(uint32_t[5]);
float (*wkQTMovieMaxTimeLoaded)(QTMovie*);
NSString *(*wkQTMovieMaxTimeLoadedChangeNotification)(void);
int (*wkQTMovieGetType)(QTMovie*);
BOOL (*wkQTMovieHasClosedCaptions)(QTMovie*);
NSURL *(*wkQTMovieResolvedURL)(QTMovie*);
void (*wkQTMovieSetShowClosedCaptions)(QTMovie*, BOOL);
void (*wkQTMovieSelectPreferredAlternates)(QTMovie*);
NSArray *(*wkQTGetSitesInMediaDownloadCache)();
void (*wkQTClearMediaDownloadCacheForSite)(NSString *site);
void (*wkQTClearMediaDownloadCache)();

void (*wkSetDragImage)(NSImage*, NSPoint offset);
bool (*wkCGContextIsPDFContext)(CGContextRef);
void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

void* wkGetHyphenationLocationBeforeIndex;

bool (*wkExecutableWasLinkedOnOrBeforeSnowLeopard)(void);

CFStringRef (*wkCopyDefaultSearchProviderDisplayName)(void);

NSCursor *(*wkCursor)(const char*);

NSArray *(*wkSpeechSynthesisGetVoiceIdentifiers)(void);
NSString *(*wkSpeechSynthesisGetDefaultVoiceIdentifierForLocale)(NSLocale *);

void (*wkUnregisterUniqueIdForElement)(id element);
void (*wkAccessibilityHandleFocusChanged)(void);
CFTypeID (*wkGetAXTextMarkerTypeID)(void);
CFTypeID (*wkGetAXTextMarkerRangeTypeID)(void);
CFTypeRef (*wkCreateAXTextMarkerRange)(CFTypeRef start, CFTypeRef end);
CFTypeRef (*wkCopyAXTextMarkerRangeStart)(CFTypeRef range);
CFTypeRef (*wkCopyAXTextMarkerRangeEnd)(CFTypeRef range);
CFTypeRef (*wkCreateAXTextMarker)(const void *bytes, size_t len);
BOOL (*wkGetBytesFromAXTextMarker)(CFTypeRef textMarker, void *bytes, size_t length);
AXUIElementRef (*wkCreateAXUIElementRef)(id element);

CFURLStorageSessionRef (*wkCreatePrivateStorageSession)(CFStringRef);
NSURLRequest* (*wkCopyRequestWithStorageSession)(CFURLStorageSessionRef, NSURLRequest*);
unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
NSArray *(*wkHTTPCookies)(CFHTTPCookieStorageRef);
NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *, NSURL *);
void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
void (*wkDeleteAllHTTPCookies)(CFHTTPCookieStorageRef);
void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);

void (*wkSetMetadataURL)(NSString *urlString, NSString *referrer, NSString *path);

#if !PLATFORM(IOS)
CGFloat (*wkNSElasticDeltaForTimeDelta)(CGFloat initialPosition, CGFloat initialVelocity, CGFloat elapsedTime);
CGFloat (*wkNSElasticDeltaForReboundDelta)(CGFloat delta);
CGFloat (*wkNSReboundDeltaForElasticDelta)(CGFloat delta);
#endif

#if ENABLE(PUBLIC_SUFFIX_LIST)
bool (*wkIsPublicSuffix)(NSString *host);
#endif

#if ENABLE(CACHE_PARTITIONING)
CFStringRef (*wkCachePartitionKey)(void);
#endif

int (*wkExernalDeviceTypeForPlayer)(AVPlayer *);
NSString *(*wkExernalDeviceDisplayNameForPlayer)(AVPlayer *);

bool (*wkQueryDecoderAvailability)(void);
