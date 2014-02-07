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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebCoreSystemInterface_h
#define WebCoreSystemInterface_h

#include <objc/objc.h>

#if PLATFORM(IOS)
#include "WebCoreSystemInterfaceIOS.h"
#endif

typedef const struct __CFString * CFStringRef;
typedef const struct __CFNumber * CFNumberRef;
typedef const struct __CFDictionary * CFDictionaryRef;
typedef struct CGPoint CGPoint;
typedef struct CGSize CGSize;
typedef struct CGRect CGRect;
typedef struct CGAffineTransform CGAffineTransform;
typedef struct CGContext *CGContextRef;
typedef struct CGImage *CGImageRef;
typedef struct CGColor *CGColorRef;
typedef struct CGFont *CGFontRef;
typedef struct CGColorSpace *CGColorSpaceRef;
typedef struct CGPattern *CGPatternRef;
typedef struct CGPath *CGMutablePathRef;
typedef unsigned short CGGlyph;
typedef struct __CFReadStream * CFReadStreamRef;
typedef struct __CFRunLoop * CFRunLoopRef;
typedef struct __CFHTTPMessage *CFHTTPMessageRef;
typedef struct _CFURLResponse *CFURLResponseRef;
typedef const struct _CFURLRequest *CFURLRequestRef;
typedef const struct __CTFont * CTFontRef;
typedef const struct __CTLine * CTLineRef;
typedef const struct __CTRun * CTRunRef;
typedef const struct __CTTypesetter * CTTypesetterRef;
typedef const struct __AXUIElement *AXUIElementRef;
#if !PLATFORM(IOS)
typedef struct _NSRange NSRange;

typedef UInt32 FMFont;
typedef FMFont ATSUFontID;
typedef UInt16 ATSGlyphRef;
#endif

#if PLATFORM(COCOA) && USE(CA)
#if !PLATFORM(IOS_SIMULATOR)
typedef struct __IOSurface *IOSurfaceRef;
#endif // !PLATFORM(IOS_SIMULATOR)
#endif

#if !PLATFORM(IOS)
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
typedef struct CGRect NSRect;
#else
typedef struct _NSPoint NSPoint;
typedef struct _NSRect NSRect;
#endif
#endif // !PLATFORM(IOS)

#if PLATFORM(IOS)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if USE(CFNETWORK)
typedef struct OpaqueCFHTTPCookieStorage*  CFHTTPCookieStorageRef;
typedef struct _CFURLProtectionSpace* CFURLProtectionSpaceRef;
typedef struct _CFURLCredential* WKCFURLCredentialRef;
typedef struct _CFURLRequest* CFMutableURLRequestRef;
typedef const struct _CFURLRequest* CFURLRequestRef;
#endif

OBJC_CLASS AVAsset;
OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSButtonCell;
OBJC_CLASS NSControl;
OBJC_CLASS NSCursor;
OBJC_CLASS NSData;
OBJC_CLASS NSDate;
OBJC_CLASS NSEvent;
OBJC_CLASS NSFont;
OBJC_CLASS NSHTTPCookie;
OBJC_CLASS NSImage;
OBJC_CLASS NSLocale;
OBJC_CLASS NSMenu;
OBJC_CLASS NSMutableURLRequest;
OBJC_CLASS NSString;
OBJC_CLASS NSTextFieldCell;
OBJC_CLASS NSURL;
OBJC_CLASS NSURLConnection;
OBJC_CLASS NSURLRequest;
OBJC_CLASS NSURLResponse;
OBJC_CLASS NSView;
OBJC_CLASS NSWindow;
OBJC_CLASS QTMovie;
OBJC_CLASS QTMovieView;

extern "C" {

// In alphabetical order.

extern void (*wkAdvanceDefaultButtonPulseAnimation)(NSButtonCell *);
extern void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef, void (^block)(CGRect rect));

extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
typedef enum {
    wkPatternTilingNoDistortion,
    wkPatternTilingConstantSpacingMinimalDistortion,
    wkPatternTilingConstantSpacing
} wkPatternTiling;
extern void (*wkCGContextResetClip)(CGContextRef);
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
extern bool (*wkCGContextDrawsWithCorrectShadowOffsets)(CGContextRef);
#endif
extern CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
extern CFReadStreamRef (*wkCreateCustomCFReadStream)(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);
extern CFStringRef (*wkCopyCFLocalizationPreferredName)(CFStringRef);
extern NSString* (*wkCopyNSURLResponseStatusLine)(NSURLResponse*);
extern CFArrayRef (*wkCopyNSURLResponseCertificateChain)(NSURLResponse*);
#if !PLATFORM(IOS)
extern void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
extern void (*wkDrawTextFieldCellFocusRing)(NSTextFieldCell*, NSRect);
extern void (*wkDrawCapsLockIndicator)(CGContextRef, CGRect);
extern void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
extern void (*wkDrawFocusRing)(CGContextRef, CGColorRef, int radius);
extern NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
extern NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
extern BOOL (*wkGetGlyphTransformedAdvances)(CGFontRef, NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
#endif
#if !PLATFORM(IOS)
extern void (*wkDrawMediaSliderTrack)(CGContextRef context, CGRect rect, float timeLoaded, float currentTime,
    float duration, unsigned state);
extern void (*wkDrawMediaUIPart)(int part, CGContextRef context, CGRect rect, unsigned state);
extern CFStringRef (*wkSignedPublicKeyAndChallengeString)(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
extern NSString* (*wkGetPreferredExtensionForMIMEType)(NSString*);
extern NSArray* (*wkGetExtensionsForMIMEType)(NSString*);
#endif
extern NSString* (*wkGetMIMETypeForExtension)(NSString*);
#if !PLATFORM(IOS)
extern ATSUFontID (*wkGetNSFontATSUFontId)(NSFont*);
extern double (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
#endif
extern NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
#if !PLATFORM(IOS)
extern BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
extern void (*wkGetWheelEventDeltas)(NSEvent*, float* deltaX, float* deltaY, BOOL* continuous);
extern UInt8 (*wkGetNSEventKeyChar)(NSEvent *);
extern BOOL (*wkHitTestMediaUIPart)(int part, CGRect bounds, CGPoint point);
extern void (*wkMeasureMediaUIPart)(int part, CGRect *bounds, CGSize *naturalSize);
extern NSView *(*wkCreateMediaUIBackgroundView)(void);

typedef enum {
    wkMediaUIControlTimeline,
    wkMediaUIControlSlider,
    wkMediaUIControlPlayPauseButton,
    wkMediaUIControlExitFullscreenButton,
    wkMediaUIControlRewindButton,
    wkMediaUIControlFastForwardButton,
    wkMediaUIControlVolumeUpButton,
    wkMediaUIControlVolumeDownButton
} wkMediaUIControlType;
extern NSControl *(*wkCreateMediaUIControl)(int);

extern void (*wkWindowSetAlpha)(NSWindow *, float);
extern void (*wkWindowSetScaledFrame)(NSWindow *, NSRect, NSRect);
extern void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*);
extern unsigned (*wkQTIncludeOnlyModernMediaFileTypes)(void);
extern int (*wkQTMovieDataRate)(QTMovie*);
extern void (*wkQTMovieDisableComponent)(uint32_t[5]);
extern float (*wkQTMovieMaxTimeLoaded)(QTMovie*);
extern NSString *(*wkQTMovieMaxTimeLoadedChangeNotification)(void);
extern float (*wkQTMovieMaxTimeSeekable)(QTMovie*);
extern int (*wkQTMovieGetType)(QTMovie*);
extern BOOL (*wkQTMovieHasClosedCaptions)(QTMovie*);
extern NSURL *(*wkQTMovieResolvedURL)(QTMovie*);
extern void (*wkQTMovieSetShowClosedCaptions)(QTMovie*, BOOL);
extern void (*wkQTMovieSelectPreferredAlternates)(QTMovie*);
extern void (*wkQTMovieViewSetDrawSynchronously)(QTMovieView*, BOOL);
extern NSArray *(*wkQTGetSitesInMediaDownloadCache)();
extern void (*wkQTClearMediaDownloadCacheForSite)(NSString *site);
extern void (*wkQTClearMediaDownloadCache)();
extern void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*, BOOL);
extern void (*wkSetCookieStoragePrivateBrowsingEnabled)(BOOL);
extern void (*wkSetDragImage)(NSImage*, NSPoint offset);
#endif
extern void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
extern void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
extern void (*wkSetBaseCTM)(CGContextRef, CGAffineTransform);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint);
extern CGAffineTransform (*wkGetUserToBaseCTM)(CGContextRef);
extern bool (*wkCGContextIsPDFContext)(CGContextRef);
extern void (*wkSetUpFontCache)();
extern void (*wkSignalCFReadStreamEnd)(CFReadStreamRef stream);
extern void (*wkSignalCFReadStreamError)(CFReadStreamRef stream, CFStreamError *error);
extern void (*wkSignalCFReadStreamHasBytes)(CFReadStreamRef stream);
extern unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);
extern int (*wkGetHTTPRequestPriority)(CFURLRequestRef);
extern void (*wkSetHTTPRequestMaximumPriority)(int maximumPriority);
extern void (*wkSetHTTPRequestPriority)(CFURLRequestRef, int priority);
extern void (*wkSetHTTPRequestMinimumFastLanePriority)(int priority);
extern void (*wkHTTPRequestEnablePipelining)(CFURLRequestRef);
extern void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
extern void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
extern CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

extern void (*wkGetGlyphsForCharacters)(CGFontRef, const UniChar[], CGGlyph[], size_t);
extern bool (*wkGetVerticalGlyphsForCharacters)(CTFontRef, const UniChar[], CGGlyph[], size_t);

extern BOOL (*wkUseSharedMediaUI)();

#if !PLATFORM(IOS)
extern void* wkGetHyphenationLocationBeforeIndex;
#endif

extern CTLineRef (*wkCreateCTLineWithUniCharProvider)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
enum {
    wkCTFontTransformApplyShaping = (1 << 0),
    wkCTFontTransformApplyPositioning = (1 << 1)
};

typedef int wkCTFontTransformOptions;

extern bool (*wkCTFontTransformGlyphs)(CTFontRef font, CGGlyph glyphs[], CGSize advances[], CFIndex count, wkCTFontTransformOptions options);
#endif

extern CTTypesetterRef (*wkCreateCTTypesetterWithUniCharProviderAndOptions)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*, CFDictionaryRef options);

extern CGSize (*wkCTRunGetInitialAdvance)(CTRunRef);

#if PLATFORM(COCOA) && USE(CA) && !PLATFORM(IOS_SIMULATOR)
extern CGContextRef (*wkIOSurfaceContextCreate)(IOSurfaceRef surface, unsigned width, unsigned height, CGColorSpaceRef colorSpace);
extern CGImageRef (*wkIOSurfaceContextCreateImage)(CGContextRef context);
#endif

#if !PLATFORM(IOS)
extern int (*wkRecommendedScrollerStyle)(void);

extern bool (*wkExecutableWasLinkedOnOrBeforeSnowLeopard)(void);

extern CFStringRef (*wkCopyDefaultSearchProviderDisplayName)(void);
extern void (*wkSetCrashReportApplicationSpecificInformation)(CFStringRef);

extern NSURL *(*wkAVAssetResolvedURL)(AVAsset*);

extern NSCursor *(*wkCursor)(const char*);
#endif // !PLATFORM(IOS)
    
#if !PLATFORM(IOS)
extern NSArray *(*wkSpeechSynthesisGetVoiceIdentifiers)(void);
extern NSString *(*wkSpeechSynthesisGetDefaultVoiceIdentifierForLocale)(NSLocale *);

extern void (*wkUnregisterUniqueIdForElement)(id element);
extern void (*wkAccessibilityHandleFocusChanged)(void);    
extern CFTypeID (*wkGetAXTextMarkerTypeID)(void);
extern CFTypeID (*wkGetAXTextMarkerRangeTypeID)(void);
extern CFTypeRef (*wkCreateAXTextMarkerRange)(CFTypeRef start, CFTypeRef end);
extern CFTypeRef (*wkCopyAXTextMarkerRangeStart)(CFTypeRef range);
extern CFTypeRef (*wkCopyAXTextMarkerRangeEnd)(CFTypeRef range);
extern CFTypeRef (*wkCreateAXTextMarker)(const void *bytes, size_t len);
extern BOOL (*wkGetBytesFromAXTextMarker)(CFTypeRef textMarker, void *bytes, size_t length);
extern AXUIElementRef (*wkCreateAXUIElementRef)(id element);
#endif // !PLATFORM(IOS)

#if PLATFORM(IOS)
extern CGSize (*wkGetViewportScreenSize)(void);
extern void (*wkSetLayerContentsScale)(CALayer *);
extern float (*wkGetScreenScaleFactor)(void);
#endif

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
extern CFURLStorageSessionRef (*wkCreatePrivateStorageSession)(CFStringRef);
extern NSURLRequest* (*wkCopyRequestWithStorageSession)(CFURLStorageSessionRef, NSURLRequest*);

typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
extern CFHTTPCookieStorageRef (*wkCopyHTTPCookieStorage)(CFURLStorageSessionRef);
extern unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
extern void (*wkSetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef, unsigned);
extern NSArray *(*wkHTTPCookies)(CFHTTPCookieStorageRef);
extern NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *, NSURL *);
extern void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
extern void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);
extern void (*wkDeleteAllHTTPCookies)(CFHTTPCookieStorageRef);

extern CFStringRef (*wkGetCFURLResponseMIMEType)(CFURLResponseRef);
extern CFURLRef (*wkGetCFURLResponseURL)(CFURLResponseRef);
extern CFHTTPMessageRef (*wkGetCFURLResponseHTTPResponse)(CFURLResponseRef);
extern CFStringRef (*wkCopyCFURLResponseSuggestedFilename)(CFURLResponseRef);
extern void (*wkSetCFURLResponseMIMEType)(CFURLResponseRef, CFStringRef mimeType);

extern void(*wkDestroyRenderingResources)(void);

#if USE(CFNETWORK)
extern CFHTTPCookieStorageRef (*wkGetDefaultHTTPCookieStorage)();
extern WKCFURLCredentialRef (*wkCopyCredentialFromCFPersistentStorage)(CFURLProtectionSpaceRef protectionSpace);
extern void (*wkSetCFURLRequestShouldContentSniff)(CFMutableURLRequestRef, bool);
extern CFArrayRef (*wkCFURLRequestCopyHTTPRequestBodyParts)(CFURLRequestRef);
extern void (*wkCFURLRequestSetHTTPRequestBodyParts)(CFMutableURLRequestRef, CFArrayRef bodyParts);
extern void (*wkSetRequestStorageSession)(CFURLStorageSessionRef, CFMutableURLRequestRef);
#endif
#if !PLATFORM(IOS)
extern void (*wkSetMetadataURL)(NSString *urlString, NSString *referrer, NSString *path);
#endif

#if !PLATFORM(IOS)
#import <dispatch/dispatch.h>

extern dispatch_source_t (*wkCreateVMPressureDispatchOnMainQueue)(void);
#endif

#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
extern dispatch_source_t (*wkCreateMemoryStatusPressureCriticalDispatchOnMainQueue)(void);
#endif
    
#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
extern bool (*wkExecutableWasLinkedOnOrBeforeLion)(void);
#endif

extern void (*wkCGPathAddRoundedRect)(CGMutablePathRef path, const CGAffineTransform* matrix, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight);

extern void (*wkCFURLRequestAllowAllPostCaching)(CFURLRequestRef);

#if !PLATFORM(IOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
extern CGFloat (*wkNSElasticDeltaForTimeDelta)(CGFloat initialPosition, CGFloat initialVelocity, CGFloat elapsedTime);
extern CGFloat (*wkNSElasticDeltaForReboundDelta)(CGFloat delta);
extern CGFloat (*wkNSReboundDeltaForElasticDelta)(CGFloat delta);
#endif

#if ENABLE(PUBLIC_SUFFIX_LIST)
extern bool (*wkIsPublicSuffix)(NSString *host);
#endif

#if ENABLE(CACHE_PARTITIONING)
extern CFStringRef (*wkCachePartitionKey)(void);
#endif
}

#endif
