/*
 * Copyright 2006, 2007, 2008 Apple Computer, Inc. All rights reserved.
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

#import "config.h"
#import "WebCoreSystemInterface.h"

void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef context, void (^block)(CGRect rect));
BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
void (*wkCGContextResetClip)(CGContextRef); 
CFStringRef (*wkCopyCFLocalizationPreferredName)(CFStringRef);
void (*wkClearGlyphVector)(void* glyphs);
OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
NSString* (*wkGetMIMETypeForExtension)(NSString*);
NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
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
NSString* (*wkCopyNSURLResponseStatusLine)(NSURLResponse*);
void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
id (*wkCreateNSURLConnectionDelegateProxy)(void);
unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);
int (*wkGetHTTPRequestPriority)(CFURLRequestRef);
void (*wkSetHTTPRequestMaximumPriority)(int priority);
void (*wkSetHTTPRequestPriority)(CFURLRequestRef, int priority);
void (*wkSetHTTPRequestMinimumFastLanePriority)(int priority);
void (*wkHTTPRequestEnablePipelining)(CFURLRequestRef);
void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
void (*wkSetCookieStoragePrivateBrowsingEnabled)(BOOL);
CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);
bool (*wkExecutableWasLinkedOnOrAfterIOSVersion)(int);
int (*wkGetDeviceClass)(void);
CGSize (*wkGetViewportScreenSize)(void);
void (*wkSetLayerContentsScale)(CALayer *);
float (*wkGetScreenScaleFactor)(void);
bool (*wkIsGB18030ComplianceRequired)(void);
void (*wkCGPathAddRoundedRect)(CGMutablePathRef path, const CGAffineTransform* matrix, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight); 
void (*wkCFURLRequestAllowAllPostCaching)(CFURLRequestRef);
CFArrayRef (*wkCopyNSURLResponseCertificateChain)(NSURLResponse*);

#if USE(CFNETWORK)
CFHTTPCookieStorageRef (*wkGetDefaultHTTPCookieStorage)();
WKCFURLCredentialRef (*wkCopyCredentialFromCFPersistentStorage)(CFURLProtectionSpaceRef protectionSpace);
void (*wkSetCFURLRequestShouldContentSniff)(CFMutableURLRequestRef, bool);
CFArrayRef (*wkCFURLRequestCopyHTTPRequestBodyParts)(CFURLRequestRef);
void (*wkCFURLRequestSetHTTPRequestBodyParts)(CFMutableURLRequestRef, CFArrayRef bodyParts);
void (*wkSetRequestStorageSession)(CFURLStorageSessionRef, CFMutableURLRequestRef);
#endif

bool (*wkGetVerticalGlyphsForCharacters)(CTFontRef, const UniChar[], CGGlyph[], size_t);
CTLineRef (*wkCreateCTLineWithUniCharProvider)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*);
bool (*wkCTFontTransformGlyphs)(CTFontRef font, CGGlyph glyphs[], CGSize advances[], CFIndex count, wkCTFontTransformOptions options);

CGSize (*wkCTRunGetInitialAdvance)(CTRunRef);

CTTypesetterRef (*wkCreateCTTypesetterWithUniCharProviderAndOptions)(const UniChar* (*provide)(CFIndex stringIndex, CFIndex* charCount, CFDictionaryRef* attributes, void*), void (*dispose)(const UniChar* chars, void*), void*, CFDictionaryRef options);

#if !PLATFORM(IOS_SIMULATOR)
CGContextRef (*wkIOSurfaceContextCreate)(IOSurfaceRef surface, unsigned width, unsigned height, CGColorSpaceRef colorSpace);
CGImageRef (*wkIOSurfaceContextCreateImage)(CGContextRef context);
#endif // !PLATFORM(IOS_SIMULATOR)

CFURLStorageSessionRef (*wkCreatePrivateStorageSession)(CFStringRef);
NSURLRequest* (*wkCopyRequestWithStorageSession)(CFURLStorageSessionRef, NSURLRequest*);
CFHTTPCookieStorageRef (*wkCopyHTTPCookieStorage)(CFURLStorageSessionRef);
unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
void (*wkSetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef, unsigned);
NSArray *(*wkHTTPCookies)(CFHTTPCookieStorageRef);
NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *);
void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
void (*wkDeleteAllHTTPCookies)(CFHTTPCookieStorageRef);
void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);

CFStringRef (*wkGetCFURLResponseMIMEType)(CFURLResponseRef);
CFURLRef (*wkGetCFURLResponseURL)(CFURLResponseRef);
CFHTTPMessageRef (*wkGetCFURLResponseHTTPResponse)(CFURLResponseRef);
CFStringRef (*wkCopyCFURLResponseSuggestedFilename)(CFURLResponseRef);
void (*wkSetCFURLResponseMIMEType)(CFURLResponseRef, CFStringRef mimeType);

void(*wkDestroyRenderingResources)(void);

bool (*wkCaptionAppearanceHasUserPreferences)(void);
bool (*wkCaptionAppearanceShowCaptionsWhenAvailable)(void);
CGColorRef(*wkCaptionAppearanceCopyForegroundColor)(void);
CGColorRef(*wkCaptionAppearanceCopyBackgroundColor)(void);
CGColorRef(*wkCaptionAppearanceCopyWindowColor)(void);
bool(*wkCaptionAppearanceGetForegroundOpacity)(CGFloat*);
bool(*wkCaptionAppearanceGetBackgroundOpacity)(CGFloat*);
bool(*wkCaptionAppearanceGetWindowOpacity)(CGFloat*);
CGFontRef(*wkCaptionAppearanceCopyFontForStyle)(int);
bool(*wkCaptionAppearanceGetRelativeCharacterSize)(CGFloat*);
int(*wkCaptionAppearanceGetTextEdgeStyle)(void);
CFStringRef(*wkCaptionAppearanceGetSettingsChangedNotification)(void);

#if ENABLE(PUBLIC_SUFFIX_LIST)
bool (*wkIsPublicSuffix)(NSString *host);
#endif

#if ENABLE(CACHE_PARTITIONING)
CFStringRef (*wkCachePartitionKey)(void);
#endif

CFStringRef (*wkGetUserAgent)(void);
CFStringRef (*wkGetDeviceName)(void);
CFStringRef (*wkGetOSNameForUserAgent)(void);
CFStringRef (*wkGetPlatformNameForNavigator)(void);
CFStringRef (*wkGetVendorNameForNavigator)(void);
