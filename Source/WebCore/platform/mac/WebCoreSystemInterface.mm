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
#import "WebCoreSystemInterface.h"
#import <Foundation/Foundation.h>

void (*wkAdvanceDefaultButtonPulseAnimation)(NSButtonCell *);
void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef context, void (^block)(CGRect rect));
BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
void (*wkCGContextResetClip)(CGContextRef);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
bool (*wkCGContextDrawsWithCorrectShadowOffsets)(CGContextRef);
#endif
CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
CFStringRef (*wkCopyCFLocalizationPreferredName)(CFStringRef);
NSString* (*wkCopyNSURLResponseStatusLine)(NSURLResponse*);
CFArrayRef (*wkCopyNSURLResponseCertificateChain)(NSURLResponse*);
NSString* (*wkCreateURLPasteboardFlavorTypeName)(void);
NSString* (*wkCreateURLNPasteboardFlavorTypeName)(void);
void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
void (*wkDrawTextFieldCellFocusRing)(NSTextFieldCell*, NSRect);
void (*wkDrawCapsLockIndicator)(CGContextRef, CGRect);
void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
void (*wkDrawFocusRing)(CGContextRef, CGColorRef, int radius);
NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
BOOL (*wkGetGlyphTransformedAdvances)(CGFontRef, NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
void (*wkDrawMediaSliderTrack)(CGContextRef context, CGRect rect, float timeLoaded, float currentTime,
    float duration, unsigned state);
BOOL (*wkHitTestMediaUIPart)(int part, CGRect bounds, CGPoint point);
void (*wkDrawMediaUIPart)(int part, CGContextRef context, CGRect rect, unsigned state);
void (*wkMeasureMediaUIPart)(int part, CGRect *bounds, CGSize *naturalSize);
NSView *(*wkCreateMediaUIBackgroundView)(void);
NSControl *(*wkCreateMediaUIControl)(int);
void (*wkWindowSetAlpha)(NSWindow *, float);
void (*wkWindowSetScaledFrame)(NSWindow *, NSRect, NSRect);
NSString* (*wkGetPreferredExtensionForMIMEType)(NSString*);
CFStringRef (*wkSignedPublicKeyAndChallengeString)(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
NSArray* (*wkGetExtensionsForMIMEType)(NSString*);
NSString* (*wkGetMIMETypeForExtension)(NSString*);
NSTimeInterval (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
void (*wkGetWheelEventDeltas)(NSEvent*, float* deltaX, float* deltaY, BOOL* continuous);
UInt8 (*wkGetNSEventKeyChar)(NSEvent *);
void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*);
unsigned (*wkQTIncludeOnlyModernMediaFileTypes)(void);
int (*wkQTMovieDataRate)(QTMovie*);
void (*wkQTMovieDisableComponent)(uint32_t[5]);
float (*wkQTMovieMaxTimeLoaded)(QTMovie*);
NSString *(*wkQTMovieMaxTimeLoadedChangeNotification)(void);
float (*wkQTMovieMaxTimeSeekable)(QTMovie*);
int (*wkQTMovieGetType)(QTMovie*);
BOOL (*wkQTMovieHasClosedCaptions)(QTMovie*);
NSURL *(*wkQTMovieResolvedURL)(QTMovie*);
void (*wkQTMovieSetShowClosedCaptions)(QTMovie*, BOOL);
void (*wkQTMovieSelectPreferredAlternates)(QTMovie*);
void (*wkQTMovieViewSetDrawSynchronously)(QTMovieView*, BOOL);
NSArray *(*wkQTGetSitesInMediaDownloadCache)();
void (*wkQTClearMediaDownloadCacheForSite)(NSString *site);
void (*wkQTClearMediaDownloadCache)();

void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*, BOOL);
void (*wkSetDragImage)(NSImage*, NSPoint offset);
void (*wkSetBaseCTM)(CGContextRef, CGAffineTransform);
void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint point);
CGAffineTransform (*wkGetUserToBaseCTM)(CGContextRef);
bool (*wkCGContextIsPDFContext)(CGContextRef);
void (*wkSetUpFontCache)();
void (*wkSignalCFReadStreamEnd)(CFReadStreamRef stream);
void (*wkSignalCFReadStreamHasBytes)(CFReadStreamRef stream);
void (*wkSignalCFReadStreamError)(CFReadStreamRef stream, CFStreamError *error);
CFReadStreamRef (*wkCreateCustomCFReadStream)(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);
void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);
int (*wkGetHTTPRequestPriority)(CFURLRequestRef);
void (*wkSetHTTPRequestMaximumPriority)(int priority);
void (*wkSetHTTPRequestPriority)(CFURLRequestRef, int priority);
void (*wkSetHTTPRequestMinimumFastLanePriority)(int priority);
void (*wkHTTPRequestEnablePipelining)(CFURLRequestRef);
void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

#if USE(CFNETWORK)
CFHTTPCookieStorageRef (*wkGetDefaultHTTPCookieStorage)();
WKCFURLCredentialRef (*wkCopyCredentialFromCFPersistentStorage)(CFURLProtectionSpaceRef protectionSpace);
void (*wkSetCFURLRequestShouldContentSniff)(CFMutableURLRequestRef, bool);
CFArrayRef (*wkCFURLRequestCopyHTTPRequestBodyParts)(CFURLRequestRef);
void (*wkCFURLRequestSetHTTPRequestBodyParts)(CFMutableURLRequestRef, CFArrayRef bodyParts);
void (*wkSetRequestStorageSession)(CFURLStorageSessionRef, CFMutableURLRequestRef);
#endif

void (*wkGetGlyphsForCharacters)(CGFontRef, const UniChar[], CGGlyph[], size_t);
bool (*wkGetVerticalGlyphsForCharacters)(CTFontRef, const UniChar[], CGGlyph[], size_t);

void* wkGetHyphenationLocationBeforeIndex;

CTLineRef (*wkCreateCTLineWithUniCharProvider)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
bool (*wkCTFontTransformGlyphs)(CTFontRef font, CGGlyph glyphs[], CGSize advances[], CFIndex count, wkCTFontTransformOptions options);
#endif

CGSize (*wkCTRunGetInitialAdvance)(CTRunRef);

CTTypesetterRef (*wkCreateCTTypesetterWithUniCharProviderAndOptions)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*, CFDictionaryRef options);

CGContextRef (*wkIOSurfaceContextCreate)(IOSurfaceRef surface, unsigned width, unsigned height, CGColorSpaceRef colorSpace);
CGImageRef (*wkIOSurfaceContextCreateImage)(CGContextRef context);

int (*wkRecommendedScrollerStyle)(void);

bool (*wkExecutableWasLinkedOnOrBeforeSnowLeopard)(void);

CFStringRef (*wkCopyDefaultSearchProviderDisplayName)(void);
void (*wkSetCrashReportApplicationSpecificInformation)(CFStringRef);

NSURL *(*wkAVAssetResolvedURL)(AVAsset*);

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
CFHTTPCookieStorageRef (*wkCopyHTTPCookieStorage)(CFURLStorageSessionRef);
unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
void (*wkSetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef, unsigned);
NSArray *(*wkHTTPCookies)(CFHTTPCookieStorageRef);
NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *, NSURL *);
void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
void (*wkDeleteAllHTTPCookies)(CFHTTPCookieStorageRef);
void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);

CFStringRef (*wkGetCFURLResponseMIMEType)(CFURLResponseRef);
CFURLRef (*wkGetCFURLResponseURL)(CFURLResponseRef);
CFHTTPMessageRef (*wkGetCFURLResponseHTTPResponse)(CFURLResponseRef);
CFStringRef (*wkCopyCFURLResponseSuggestedFilename)(CFURLResponseRef);
void (*wkSetCFURLResponseMIMEType)(CFURLResponseRef, CFStringRef mimeType);
void (*wkSetMetadataURL)(NSString *urlString, NSString *referrer, NSString *path);

void(*wkDestroyRenderingResources)(void);

dispatch_source_t (*wkCreateVMPressureDispatchOnMainQueue)(void);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
dispatch_source_t (*wkCreateMemoryStatusPressureCriticalDispatchOnMainQueue)(void);
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
bool (*wkExecutableWasLinkedOnOrBeforeLion)(void);
#endif

void (*wkCGPathAddRoundedRect)(CGMutablePathRef path, const CGAffineTransform* matrix, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight);

void (*wkCFURLRequestAllowAllPostCaching)(CFURLRequestRef);

#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
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
