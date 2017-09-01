/*
 * Copyright 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <objc/objc.h>

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
typedef double NSTimeInterval;
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

#if USE(CFURLCONNECTION)
typedef struct OpaqueCFHTTPCookieStorage*  CFHTTPCookieStorageRef;
typedef struct _CFURLProtectionSpace* CFURLProtectionSpaceRef;
typedef struct _CFURLCredential* WKCFURLCredentialRef;
typedef struct _CFURLRequest* CFMutableURLRequestRef;
typedef const struct _CFURLRequest* CFURLRequestRef;
#endif

OBJC_CLASS AVAsset;
OBJC_CLASS AVPlayer;
OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSButtonCell;
OBJC_CLASS NSCell;
OBJC_CLASS NSControl;
OBJC_CLASS NSCursor;
OBJC_CLASS NSData;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
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

extern void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef, void (^block)(CGRect rect));

typedef enum {
    wkPatternTilingNoDistortion,
    wkPatternTilingConstantSpacingMinimalDistortion,
    wkPatternTilingConstantSpacing
} wkPatternTiling;
#if !PLATFORM(IOS)
extern void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
extern void (*wkDrawMediaSliderTrack)(CGContextRef context, CGRect rect, float timeLoaded, float currentTime,
    float duration, unsigned state);
extern void (*wkDrawMediaUIPart)(int part, CGContextRef context, CGRect rect, unsigned state);
extern double (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
extern BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
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

extern unsigned (*wkQTIncludeOnlyModernMediaFileTypes)(void);
extern void (*wkQTMovieDisableComponent)(uint32_t[5]);
extern float (*wkQTMovieMaxTimeLoaded)(QTMovie*);
extern NSString *(*wkQTMovieMaxTimeLoadedChangeNotification)(void);
extern int (*wkQTMovieGetType)(QTMovie*);
extern BOOL (*wkQTMovieHasClosedCaptions)(QTMovie*);
extern NSURL *(*wkQTMovieResolvedURL)(QTMovie*);
extern void (*wkQTMovieSetShowClosedCaptions)(QTMovie*, BOOL);
extern void (*wkQTMovieSelectPreferredAlternates)(QTMovie*);
extern NSArray *(*wkQTGetSitesInMediaDownloadCache)();
extern void (*wkQTClearMediaDownloadCacheForSite)(NSString *site);
extern void (*wkQTClearMediaDownloadCache)();
extern void (*wkSetCookieStoragePrivateBrowsingEnabled)(BOOL);
#endif
extern void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
extern void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
extern CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);

#if !PLATFORM(IOS)
extern void* wkGetHyphenationLocationBeforeIndex;
#endif

#if !PLATFORM(IOS)
extern bool (*wkExecutableWasLinkedOnOrBeforeSnowLeopard)(void);

extern CFStringRef (*wkCopyDefaultSearchProviderDisplayName)(void);

extern NSCursor *(*wkCursor)(const char*);
#endif // !PLATFORM(IOS)
    
#if !PLATFORM(IOS)
extern NSArray *(*wkSpeechSynthesisGetVoiceIdentifiers)(void);
extern NSString *(*wkSpeechSynthesisGetDefaultVoiceIdentifierForLocale)(NSLocale *);
#endif // !PLATFORM(IOS)

#if PLATFORM(IOS)
extern void (*wkSetLayerContentsScale)(CALayer *);
#endif

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;

#if !PLATFORM(IOS)
extern CGFloat (*wkNSElasticDeltaForTimeDelta)(CGFloat initialPosition, CGFloat initialVelocity, CGFloat elapsedTime);
extern CGFloat (*wkNSElasticDeltaForReboundDelta)(CGFloat delta);
extern CGFloat (*wkNSReboundDeltaForElasticDelta)(CGFloat delta);
#endif

#if ENABLE(PUBLIC_SUFFIX_LIST)
extern bool (*wkIsPublicSuffix)(NSString *host);
#endif

extern CFStringRef (*wkCachePartitionKey)(void);

typedef enum {
    wkExternalPlaybackTypeNone,
    wkExternalPlaybackTypeAirPlay,
    wkExternalPlaybackTypeTVOut,
} wkExternalPlaybackType;
extern int (*wkExernalDeviceTypeForPlayer)(AVPlayer *);
extern NSString *(*wkExernalDeviceDisplayNameForPlayer)(AVPlayer *);

extern bool (*wkQueryDecoderAvailability)(void);

}
