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

#include <ApplicationServices/ApplicationServices.h>
#include <objc/objc.h>

#if PLATFORM(MAC) && USE(CA) && !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
#include <IOSurface/IOSurface.h>
#endif

typedef struct _NSRange NSRange;

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
typedef struct CGRect NSRect;
#else
typedef struct _NSPoint NSPoint;
typedef struct _NSRect NSRect;
#endif

#ifdef __OBJC__
@class NSArray;
@class NSButtonCell;
@class NSData;
@class NSDate;
@class NSEvent;
@class NSFont;
@class NSHTTPCookie;
@class NSImage;
@class NSMenu;
@class NSMutableURLRequest;
@class NSString;
@class NSTextFieldCell;
@class NSURL;
@class NSURLConnection;
@class NSURLRequest;
@class NSURLResponse;
@class NSView;
@class QTMovie;
@class QTMovieView;
#else
class NSArray;
class NSButtonCell;
class NSData;
class NSDate;
class NSEvent;
class NSFont;
class NSHTTPCookie;
class NSImage;
class NSMenu;
class NSMutableArray;
class NSMutableURLRequest;
class NSURL;
class NSURLRequest;
class NSString;
class NSTextFieldCell;
class NSURLConnection;
class NSURLResponse;
class NSView;
class QTMovie;
class QTMovieView;
#endif

typedef struct _CFURLResponse *CFURLResponseRef;

extern "C" {

// In alphabetical order.

extern void (*wkAdvanceDefaultButtonPulseAnimation)(NSButtonCell *);
extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
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
extern id (*wkCreateNSURLConnectionDelegateProxy)(void);
extern void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
extern void (*wkDrawTextFieldCellFocusRing)(NSTextFieldCell*, NSRect);
extern void (*wkDrawCapsLockIndicator)(CGContextRef, CGRect);
extern void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
extern void (*wkDrawFocusRing)(CGContextRef, CGColorRef, int radius);
extern NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
extern NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
extern BOOL (*wkGetGlyphTransformedAdvances)(CGFontRef, NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
extern void (*wkDrawMediaSliderTrack)(int themeStyle, CGContextRef context, CGRect rect, float timeLoaded, float currentTime, 
    float duration, unsigned state);
extern void (*wkDrawMediaUIPart)(int part, int themeStyle, CGContextRef context, CGRect rect, unsigned state);
extern CFStringRef (*wkSignedPublicKeyAndChallengeString)(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
extern NSString* (*wkGetPreferredExtensionForMIMEType)(NSString*);
extern NSArray* (*wkGetExtensionsForMIMEType)(NSString*);
extern NSString* (*wkGetMIMETypeForExtension)(NSString*);
extern ATSUFontID (*wkGetNSFontATSUFontId)(NSFont*);
extern double (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
extern NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
extern BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
extern void (*wkGetWheelEventDeltas)(NSEvent*, float* deltaX, float* deltaY, BOOL* continuous);
extern BOOL (*wkHitTestMediaUIPart)(int part, int themeStyle, CGRect bounds, CGPoint point);
extern void (*wkMeasureMediaUIPart)(int part, int themeStyle, CGRect *bounds, CGSize *naturalSize);
extern BOOL (*wkMediaControllerThemeAvailable)(int themeStyle);
extern void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*);
extern unsigned (*wkQTIncludeOnlyModernMediaFileTypes)(void);
extern int (*wkQTMovieDataRate)(QTMovie*);
extern void (*wkQTMovieDisableComponent)(uint32_t[5]);
extern float (*wkQTMovieMaxTimeLoaded)(QTMovie*);
extern NSString *(*wkQTMovieMaxTimeLoadedChangeNotification)(void);
extern float (*wkQTMovieMaxTimeSeekable)(QTMovie*);
extern int (*wkQTMovieGetType)(QTMovie*);
extern BOOL (*wkQTMovieHasClosedCaptions)(QTMovie*);
extern void (*wkQTMovieSetShowClosedCaptions)(QTMovie*, BOOL);
extern void (*wkQTMovieSelectPreferredAlternates)(QTMovie*);
extern void (*wkQTMovieViewSetDrawSynchronously)(QTMovieView*, BOOL);
extern void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*);
extern void (*wkSetCookieStoragePrivateBrowsingEnabled)(BOOL);
extern void (*wkSetDragImage)(NSImage*, NSPoint offset);
extern void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
extern void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
extern void (*wkSetPatternBaseCTM)(CGContextRef, CGAffineTransform);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint);
extern CGAffineTransform (*wkGetUserToBaseCTM)(CGContextRef);
extern void (*wkSetUpFontCache)();
extern void (*wkSignalCFReadStreamEnd)(CFReadStreamRef stream);
extern void (*wkSignalCFReadStreamError)(CFReadStreamRef stream, CFStreamError *error);
extern void (*wkSignalCFReadStreamHasBytes)(CFReadStreamRef stream);
extern unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);
extern int (*wkGetHTTPPipeliningPriority)(NSURLRequest *);
extern void (*wkSetHTTPPipeliningMaximumPriority)(int maximumPriority);
extern void (*wkSetHTTPPipeliningPriority)(NSMutableURLRequest *, int priority);
extern void (*wkSetHTTPPipeliningMinimumFastLanePriority)(int priority);
extern void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
extern void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
extern CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL);

#ifndef BUILDING_ON_TIGER
extern void (*wkGetGlyphsForCharacters)(CGFontRef, const UniChar[], CGGlyph[], size_t);
#else
#define GLYPH_VECTOR_SIZE (50 * 32)

extern void (*wkClearGlyphVector)(void* glyphs);
extern OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
extern CFStringRef (*wkCopyFullFontName)(CGFontRef font);
extern OSStatus (*wkGetATSStyleGroup)(ATSUStyle, void** styleGroup);
extern CGFontRef (*wkGetCGFontFromNSFont)(NSFont*);
extern void (*wkGetFontMetrics)(CGFontRef, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
extern ATSLayoutRecord* (*wkGetGlyphVectorFirstRecord)(void* glyphVector);
extern void* wkGetGlyphsForCharacters;
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
extern void (*wkReleaseStyleGroup)(void* group);
extern BOOL (*wkSupportsMultipartXMixedReplace)(NSMutableURLRequest *);
#endif

extern BOOL (*wkUseSharedMediaUI)();

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
extern void* wkGetHyphenationLocationBeforeIndex;
#else
extern CFIndex (*wkGetHyphenationLocationBeforeIndex)(CFStringRef string, CFIndex index);

typedef enum {
    wkEventPhaseNone = 0,
    wkEventPhaseBegan = 1,
    wkEventPhaseChanged = 2,
    wkEventPhaseEnded = 3,
} wkEventPhase;

extern int (*wkGetNSEventMomentumPhase)(NSEvent *);
#endif

extern CTLineRef (*wkCreateCTLineWithUniCharProvider)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
extern CTTypesetterRef (*wkCreateCTTypesetterWithUniCharProviderAndOptions)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*, CFDictionaryRef options);

extern CGContextRef (*wkIOSurfaceContextCreate)(IOSurfaceRef surface, unsigned width, unsigned height, CGColorSpaceRef colorSpace);
extern CGImageRef (*wkIOSurfaceContextCreateImage)(CGContextRef context);

typedef struct __WKScrollbarPainter *WKScrollbarPainterRef;
typedef struct __WKScrollbarPainterController *WKScrollbarPainterControllerRef;

extern WKScrollbarPainterRef (*wkMakeScrollbarPainter)(int controlSize, bool isHorizontal);
extern WKScrollbarPainterRef (*wkMakeScrollbarReplacementPainter)(WKScrollbarPainterRef oldPainter, int newStyle, int controlSize, bool isHorizontal);
extern void (*wkScrollbarPainterSetDelegate)(WKScrollbarPainterRef, id scrollbarPainterDelegate);
extern void (*wkScrollbarPainterPaint)(WKScrollbarPainterRef, bool enabled, double value, CGFloat proportion, CGRect frameRect);
extern void (*wkScrollbarPainterForceFlashScrollers)(WKScrollbarPainterControllerRef);
extern int (*wkScrollbarThickness)(int controlSize);
extern int (*wkScrollbarMinimumThumbLength)(WKScrollbarPainterRef);
extern int (*wkScrollbarMinimumTotalLengthNeededForThumb)(WKScrollbarPainterRef);
extern CGFloat (*wkScrollbarPainterKnobAlpha)(WKScrollbarPainterRef);
extern void (*wkSetScrollbarPainterKnobAlpha)(WKScrollbarPainterRef, CGFloat);
extern CGFloat (*wkScrollbarPainterTrackAlpha)(WKScrollbarPainterRef);
extern void (*wkSetScrollbarPainterTrackAlpha)(WKScrollbarPainterRef, CGFloat);
extern bool (*wkScrollbarPainterIsHorizontal)(WKScrollbarPainterRef);
extern void (*wkScrollbarPainterSetOverlayState)(WKScrollbarPainterRef, int overlayScrollerState);

extern WKScrollbarPainterControllerRef (*wkMakeScrollbarPainterController)(id painterControllerDelegate);
extern void (*wkSetPainterForPainterController)(WKScrollbarPainterControllerRef, WKScrollbarPainterRef, bool isHorizontal);
extern WKScrollbarPainterRef (*wkVerticalScrollbarPainterForController)(WKScrollbarPainterControllerRef);
extern WKScrollbarPainterRef (*wkHorizontalScrollbarPainterForController)(WKScrollbarPainterControllerRef);
extern void (*wkSetScrollbarPainterControllerStyle)(WKScrollbarPainterControllerRef, int newStyle);
extern void (*wkContentAreaScrolled)(WKScrollbarPainterControllerRef);
extern void (*wkContentAreaWillPaint)(WKScrollbarPainterControllerRef);
extern void (*wkMouseEnteredContentArea)(WKScrollbarPainterControllerRef);
extern void (*wkMouseExitedContentArea)(WKScrollbarPainterControllerRef);
extern void (*wkMouseMovedInContentArea)(WKScrollbarPainterControllerRef);
extern void (*wkWillStartLiveResize)(WKScrollbarPainterControllerRef);
extern void (*wkContentAreaResized)(WKScrollbarPainterControllerRef);
extern void (*wkWillEndLiveResize)(WKScrollbarPainterControllerRef);
extern void (*wkContentAreaDidShow)(WKScrollbarPainterControllerRef);
extern void (*wkContentAreaDidHide)(WKScrollbarPainterControllerRef);
extern void (*wkDidBeginScrollGesture)(WKScrollbarPainterControllerRef);
extern void (*wkDidEndScrollGesture)(WKScrollbarPainterControllerRef);

extern bool (*wkScrollbarPainterUsesOverlayScrollers)(void);
#endif

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

typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
extern CFURLStorageSessionRef (*wkCreatePrivateStorageSession)(CFStringRef);
extern NSURLRequest* (*wkCopyRequestWithStorageSession)(CFURLStorageSessionRef, NSURLRequest*);

typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
extern CFHTTPCookieStorageRef (*wkCopyHTTPCookieStorage)(CFURLStorageSessionRef);
extern unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
extern NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *);
extern void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
extern void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);

extern CFStringRef (*wkGetCFURLResponseMIMEType)(CFURLResponseRef);
extern CFURLRef (*wkGetCFURLResponseURL)(CFURLResponseRef);
extern CFHTTPMessageRef (*wkGetCFURLResponseHTTPResponse)(CFURLResponseRef);
extern CFStringRef (*wkCopyCFURLResponseSuggestedFilename)(CFURLResponseRef);
extern void (*wkSetCFURLResponseMIMEType)(CFURLResponseRef, CFStringRef mimeType);

}

#endif
