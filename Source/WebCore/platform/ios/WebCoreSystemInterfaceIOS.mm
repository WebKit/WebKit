/*
 * Copyright 2006, 2007, 2008 Apple Inc. All rights reserved.
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

WEBCORE_EXPORT void (*wkCALayerEnumerateRectsBeingDrawnWithBlock)(CALayer *, CGContextRef context, void (^block)(CGRect rect));
WEBCORE_EXPORT BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
WEBCORE_EXPORT CGPatternRef (*wkCGPatternCreateWithImageAndTransform)(CGImageRef, CGAffineTransform, int);
WEBCORE_EXPORT void (*wkCGContextResetClip)(CGContextRef);
WEBCORE_EXPORT void (*wkClearGlyphVector)(void* glyphs);
WEBCORE_EXPORT OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
WEBCORE_EXPORT NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
WEBCORE_EXPORT void (*wkSetBaseCTM)(CGContextRef, CGAffineTransform);
WEBCORE_EXPORT void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint point);
WEBCORE_EXPORT CGAffineTransform (*wkGetUserToBaseCTM)(CGContextRef);
WEBCORE_EXPORT bool (*wkCGContextIsPDFContext)(CGContextRef);
WEBCORE_EXPORT NSString* (*wkCopyNSURLResponseStatusLine)(NSURLResponse*);
WEBCORE_EXPORT void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
WEBCORE_EXPORT void (*wkSetNSURLRequestShouldContentSniff)(NSMutableURLRequest *, BOOL);
WEBCORE_EXPORT id (*wkCreateNSURLConnectionDelegateProxy)(void);
WEBCORE_EXPORT unsigned (*wkInitializeMaximumHTTPConnectionCountPerHost)(unsigned preferredConnectionCount);
WEBCORE_EXPORT int (*wkGetHTTPRequestPriority)(CFURLRequestRef);
WEBCORE_EXPORT void (*wkSetHTTPRequestMaximumPriority)(int priority);
WEBCORE_EXPORT void (*wkSetHTTPRequestPriority)(CFURLRequestRef, int priority);
WEBCORE_EXPORT void (*wkSetHTTPRequestMinimumFastLanePriority)(int priority);
WEBCORE_EXPORT void (*wkHTTPRequestEnablePipelining)(CFURLRequestRef);
WEBCORE_EXPORT void (*wkSetCONNECTProxyForStream)(CFReadStreamRef, CFStringRef proxyHost, CFNumberRef proxyPort);
WEBCORE_EXPORT void (*wkSetCONNECTProxyAuthorizationForStream)(CFReadStreamRef, CFStringRef proxyAuthorizationString);
WEBCORE_EXPORT void (*wkSetCookieStoragePrivateBrowsingEnabled)(BOOL);
WEBCORE_EXPORT CFHTTPMessageRef (*wkCopyCONNECTProxyResponse)(CFReadStreamRef, CFURLRef responseURL, CFStringRef proxyHost, CFNumberRef proxyPort);
WEBCORE_EXPORT bool (*wkExecutableWasLinkedOnOrAfterIOSVersion)(int);
WEBCORE_EXPORT int (*wkGetDeviceClass)(void);
WEBCORE_EXPORT CGSize (*wkGetScreenSize)(void);
WEBCORE_EXPORT CGSize (*wkGetAvailableScreenSize)(void);
WEBCORE_EXPORT void (*wkSetLayerContentsScale)(CALayer *);
WEBCORE_EXPORT float (*wkGetScreenScaleFactor)(void);
WEBCORE_EXPORT bool (*wkIsGB18030ComplianceRequired)(void);
WEBCORE_EXPORT void (*wkCFURLRequestAllowAllPostCaching)(CFURLRequestRef);
WEBCORE_EXPORT CFArrayRef (*wkCopyNSURLResponseCertificateChain)(NSURLResponse*);
WEBCORE_EXPORT CFStringEncoding (*wkGetWebDefaultCFStringEncoding)(void);

#if USE(CFNETWORK)
WEBCORE_EXPORT CFHTTPCookieStorageRef (*wkGetDefaultHTTPCookieStorage)();
WEBCORE_EXPORT WKCFURLCredentialRef (*wkCopyCredentialFromCFPersistentStorage)(CFURLProtectionSpaceRef protectionSpace);
WEBCORE_EXPORT void (*wkSetCFURLRequestShouldContentSniff)(CFMutableURLRequestRef, bool);
WEBCORE_EXPORT void (*wkSetRequestStorageSession)(CFURLStorageSessionRef, CFMutableURLRequestRef);
#endif

WEBCORE_EXPORT CFURLStorageSessionRef (*wkCreatePrivateStorageSession)(CFStringRef);
WEBCORE_EXPORT NSURLRequest* (*wkCopyRequestWithStorageSession)(CFURLStorageSessionRef, NSURLRequest*);
WEBCORE_EXPORT CFHTTPCookieStorageRef (*wkCopyHTTPCookieStorage)(CFURLStorageSessionRef);
WEBCORE_EXPORT unsigned (*wkGetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef);
WEBCORE_EXPORT void (*wkSetHTTPCookieAcceptPolicy)(CFHTTPCookieStorageRef, unsigned);
WEBCORE_EXPORT NSArray *(*wkHTTPCookies)(CFHTTPCookieStorageRef);
WEBCORE_EXPORT NSArray *(*wkHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSURL *, NSURL *);
WEBCORE_EXPORT void (*wkSetHTTPCookiesForURL)(CFHTTPCookieStorageRef, NSArray *, NSURL *, NSURL *);
WEBCORE_EXPORT void (*wkDeleteAllHTTPCookies)(CFHTTPCookieStorageRef);
WEBCORE_EXPORT void (*wkDeleteHTTPCookie)(CFHTTPCookieStorageRef, NSHTTPCookie *);

WEBCORE_EXPORT CFStringRef (*wkGetCFURLResponseMIMEType)(CFURLResponseRef);
WEBCORE_EXPORT CFURLRef (*wkGetCFURLResponseURL)(CFURLResponseRef);
WEBCORE_EXPORT CFHTTPMessageRef (*wkGetCFURLResponseHTTPResponse)(CFURLResponseRef);
WEBCORE_EXPORT CFStringRef (*wkCopyCFURLResponseSuggestedFilename)(CFURLResponseRef);
WEBCORE_EXPORT void (*wkSetCFURLResponseMIMEType)(CFURLResponseRef, CFStringRef mimeType);

WEBCORE_EXPORT void(*wkDestroyRenderingResources)(void);

WEBCORE_EXPORT bool (*wkCaptionAppearanceHasUserPreferences)(void);
WEBCORE_EXPORT bool (*wkCaptionAppearanceShowCaptionsWhenAvailable)(void);
WEBCORE_EXPORT CGColorRef(*wkCaptionAppearanceCopyForegroundColor)(void);
WEBCORE_EXPORT CGColorRef(*wkCaptionAppearanceCopyBackgroundColor)(void);
WEBCORE_EXPORT CGColorRef(*wkCaptionAppearanceCopyWindowColor)(void);
WEBCORE_EXPORT bool(*wkCaptionAppearanceGetForegroundOpacity)(CGFloat*);
WEBCORE_EXPORT bool(*wkCaptionAppearanceGetBackgroundOpacity)(CGFloat*);
WEBCORE_EXPORT bool(*wkCaptionAppearanceGetWindowOpacity)(CGFloat*);
WEBCORE_EXPORT CGFontRef(*wkCaptionAppearanceCopyFontForStyle)(int);
WEBCORE_EXPORT bool(*wkCaptionAppearanceGetRelativeCharacterSize)(CGFloat*);
WEBCORE_EXPORT int(*wkCaptionAppearanceGetTextEdgeStyle)(void);
WEBCORE_EXPORT CFStringRef(*wkCaptionAppearanceGetSettingsChangedNotification)(void);

#if ENABLE(PUBLIC_SUFFIX_LIST)
WEBCORE_EXPORT bool (*wkIsPublicSuffix)(NSString *host);
#endif

#if ENABLE(CACHE_PARTITIONING)
WEBCORE_EXPORT CFStringRef (*wkCachePartitionKey)(void);
#endif

WEBCORE_EXPORT CFStringRef (*wkGetUserAgent)(void);
WEBCORE_EXPORT CFStringRef (*wkGetDeviceName)(void);
WEBCORE_EXPORT CFStringRef (*wkGetOSNameForUserAgent)(void);
WEBCORE_EXPORT CFStringRef (*wkGetPlatformNameForNavigator)(void);
WEBCORE_EXPORT CFStringRef (*wkGetVendorNameForNavigator)(void);

WEBCORE_EXPORT bool (*wkIsOptimizedFullscreenSupported)(void);

WEBCORE_EXPORT int (*wkExernalDeviceTypeForPlayer)(AVPlayer *);
WEBCORE_EXPORT NSString *(*wkExernalDeviceDisplayNameForPlayer)(AVPlayer *);

WEBCORE_EXPORT bool (*wkQueryDecoderAvailability)(void);
