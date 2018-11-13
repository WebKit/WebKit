/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma clang system_header

#pragma once

#include <CFNetwork/CFNetwork.h>
#include <pal/spi/cf/CFNetworkConnectionCacheSPI.h>

#if ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101302 && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110200) || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED >= 40200) || (PLATFORM(TVOS) && __TV_OS_VERSION_MIN_REQUIRED >= 110200))
#define USE_CFNETWORK_IGNORE_HSTS 1
#endif

#if PLATFORM(WIN) || USE(APPLE_INTERNAL_SDK)

#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CFNetwork/CFProxySupportPriv.h>
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLConnectionPriv.h>
#include <CFNetwork/CFURLCredentialStorage.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <CFNetwork/CFURLRequestPriv.h>
#include <CFNetwork/CFURLResponsePriv.h>
#include <CFNetwork/CFURLStorageSession.h>

// FIXME: Remove the defined(__OBJC__)-guard once we fix <rdar://problem/19033610>.
#if defined(__OBJC__) && PLATFORM(COCOA)
#import <CFNetwork/CFNSURLConnection.h>
#endif

#else // !PLATFORM(WIN) && !USE(APPLE_INTERNAL_SDK)

typedef CF_ENUM(int64_t, _TimingDataOptions)
{
    _TimingDataOptionsEnableW3CNavigationTiming = (1 << 0)
};

enum CFURLCacheStoragePolicy {
    kCFURLCacheStorageAllowed = 0,
    kCFURLCacheStorageAllowedInMemoryOnly = 1,
    kCFURLCacheStorageNotAllowed = 2
};
typedef enum CFURLCacheStoragePolicy CFURLCacheStoragePolicy;

typedef const struct _CFCachedURLResponse* CFCachedURLResponseRef;
typedef const struct _CFURLCache* CFURLCacheRef;
typedef const struct _CFURLCredential* CFURLCredentialRef;
typedef const struct _CFURLRequest* CFURLRequestRef;
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
typedef const struct __CFData* CFDataRef;
typedef const struct OpaqueCFHTTPCookie* CFHTTPCookieRef;
typedef struct _CFURLConnection* CFURLConnectionRef;
typedef struct _CFURLCredentialStorage* CFURLCredentialStorageRef;
typedef struct _CFURLProtectionSpace* CFURLProtectionSpaceRef;
typedef struct _CFURLRequest* CFMutableURLRequestRef;
typedef struct _CFURLResponse* CFURLResponseRef;
typedef struct OpaqueCFHTTPCookieStorage* CFHTTPCookieStorageRef;
typedef CFIndex CFURLRequestPriority;
typedef int CFHTTPCookieStorageAcceptPolicy;

#ifdef __BLOCKS__
typedef void (^CFCachedURLResponseCallBackBlock)(CFCachedURLResponseRef);
#endif

#if defined(__OBJC__)

@interface NSURLCache ()
- (CFURLCacheRef)_CFURLCache;
@end

@interface NSURLRequest ()
+ (NSArray *)allowsSpecificHTTPSCertificateForHost:(NSString *)host;
+ (void)setAllowsSpecificHTTPSCertificate:(NSArray *)allow forHost:(NSString *)host;
+ (void)setDefaultTimeoutInterval:(NSTimeInterval)seconds;
- (NSArray *)contentDispositionEncodingFallbackArray;
- (CFMutableURLRequestRef)_CFURLRequest;
- (id)_initWithCFURLRequest:(CFURLRequestRef)request;
- (id)_propertyForKey:(NSString *)key;
- (void)_setProperty:(id)value forKey:(NSString *)key;
#if USE(CFNETWORK_IGNORE_HSTS)
- (BOOL)_schemeWasUpgradedDueToDynamicHSTS;
- (BOOL)_preventHSTSStorage;
- (BOOL)_ignoreHSTS;
#endif
@end

@interface NSMutableURLRequest ()
- (void)setContentDispositionEncodingFallbackArray:(NSArray *)theEncodingFallbackArray;
- (void)setBoundInterfaceIdentifier:(NSString *)identifier;
#if USE(CFNETWORK_IGNORE_HSTS)
- (void)_setPreventHSTSStorage:(BOOL)preventHSTSStorage;
- (void)_setIgnoreHSTS:(BOOL)ignoreHSTS;
#endif
@end

@interface NSURLResponse ()
+ (NSURLResponse *)_responseWithCFURLResponse:(CFURLResponseRef)response;
- (CFURLResponseRef)_CFURLResponse;
- (NSDate *)_lastModifiedDate;
@end

@interface NSHTTPCookie ()
- (CFHTTPCookieRef)_CFHTTPCookie;
+ (CFArrayRef __nullable)_ns2cfCookies:(NSArray * __nullable)nsCookies CF_RETURNS_RETAINED;
- (CFHTTPCookieRef __nullable)_GetInternalCFHTTPCookie;
@end

@interface NSURLSessionConfiguration ()
@property (assign) _TimingDataOptions _timingDataOptions;
@property (copy) NSData *_sourceApplicationAuditTokenData;
@property (nullable, copy) NSString *_sourceApplicationBundleIdentifier;
@property (nullable, copy) NSString *_sourceApplicationSecondaryIdentifier;
@property BOOL _shouldSkipPreferredClientCertificateLookup NS_AVAILABLE(10_10, 8_0);
#if PLATFORM(IOS)
@property (nullable, copy) NSString *_CTDataConnectionServiceType;
#endif
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
@property (nullable, copy) NSSet *_suppressedAutoAddedHTTPHeaders;
#endif
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
@property (copy) NSDictionary *_socketStreamProperties;
#endif
@end

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
@interface NSURLSessionTaskTransactionMetrics ()
@property (copy, readonly) NSString* _remoteAddressAndPort;
@property (copy, readonly) NSUUID* _connectionIdentifier;
@property (assign, readonly) NSInteger _requestHeaderBytesSent;
@property (assign, readonly) NSInteger _responseHeaderBytesReceived;
@property (assign, readonly) int64_t _responseBodyBytesReceived;
@property (assign, readonly) int64_t _responseBodyBytesDecoded;
@end
#endif

@interface NSHTTPCookie ()
@property (nullable, readonly, copy) NSString *_storagePartition;
@end

@interface NSHTTPCookieStorage ()
- (id)_initWithIdentifier:(NSString *)identifier private:(bool)isPrivate;
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition completionHandler:(void (^)(NSArray *))completionHandler;
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition policyProperties:(NSDictionary*)props completionHandler:(void (^)(NSArray *))completionHandler;
- (void)_setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL policyProperties:(NSDictionary*) props;
@end

@interface NSURLSessionTask ()
- (NSDictionary *)_timingData;
@property (readwrite, copy) NSString *_pathToDownloadTaskFile;
@property (copy) NSString *_storagePartitionIdentifier;
@property (nullable, readwrite, retain) NSURL *_siteForCookies;
@property (readwrite) BOOL _isTopLevelNavigation;
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
@property (nonatomic, assign) BOOL _preconnect;
#endif
@end

#endif // defined(__OBJC__)

#endif // !PLATFORM(WIN) && !USE(APPLE_INTERNAL_SDK)

#if defined(__OBJC__)
@interface NSHTTPCookieStorage ()
@property (nonatomic, readwrite) BOOL _overrideSessionCookieAcceptPolicy;
@end
#endif

WTF_EXTERN_C_BEGIN

#if !PLATFORM(WIN)

CFURLStorageSessionRef _CFURLStorageSessionCreate(CFAllocatorRef, CFStringRef, CFDictionaryRef);
CFURLCacheRef _CFURLStorageSessionCopyCache(CFAllocatorRef, CFURLStorageSessionRef);

void CFURLRequestSetShouldStartSynchronously(CFURLRequestRef, Boolean);

CFURLCacheRef CFURLCacheCopySharedURLCache();
void CFURLCacheSetMemoryCapacity(CFURLCacheRef, CFIndex memoryCapacity);
CFIndex CFURLCacheMemoryCapacity(CFURLCacheRef);
void CFURLCacheSetDiskCapacity(CFURLCacheRef, CFIndex);
CFCachedURLResponseRef CFURLCacheCopyResponseForRequest(CFURLCacheRef, CFURLRequestRef);

#if PLATFORM(COCOA)
Boolean _CFNetworkIsKnownHSTSHostWithSession(CFURLRef, CFURLStorageSessionRef);
void _CFNetworkResetHSTSHostsWithSession(CFURLStorageSessionRef);
#endif

void CFHTTPCookieStorageDeleteAllCookies(CFHTTPCookieStorageRef);
void _CFHTTPCookieStorageFlushCookieStores();

#if PLATFORM(COCOA)
CFDataRef _CFCachedURLResponseGetMemMappedData(CFCachedURLResponseRef);
#endif

#if PLATFORM(COCOA) && defined(__BLOCKS__)
void _CFCachedURLResponseSetBecameFileBackedCallBackBlock(CFCachedURLResponseRef, CFCachedURLResponseCallBackBlock, dispatch_queue_t);
#endif

extern CFStringRef const kCFHTTPCookieLocalFileDomain;
extern const CFStringRef kCFHTTPVersion1_1;
extern const CFStringRef kCFURLRequestAllowAllPOSTCaching;
extern const CFStringRef _kCFURLCachePartitionKey;
extern const CFStringRef _kCFURLConnectionPropertyShouldSniff;
extern const CFStringRef _kCFURLStorageSessionIsPrivate;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101302
extern const CFStringRef kCFURLRequestContentDecoderSkipURLCheck;
#endif

CFHTTPCookieStorageRef _CFHTTPCookieStorageGetDefault(CFAllocatorRef);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromFile(CFAllocatorRef, CFURLRef, CFHTTPCookieStorageRef);
void CFHTTPCookieStorageScheduleWithRunLoop(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef);
void CFHTTPCookieStorageSetCookie(CFHTTPCookieStorageRef, CFHTTPCookieRef);
void CFHTTPCookieStorageSetCookieAcceptPolicy(CFHTTPCookieStorageRef, CFHTTPCookieStorageAcceptPolicy);
CFHTTPCookieStorageAcceptPolicy CFHTTPCookieStorageGetCookieAcceptPolicy(CFHTTPCookieStorageRef);

typedef void (*CFHTTPCookieStorageChangedProcPtr)(CFHTTPCookieStorageRef, void*);
void CFHTTPCookieStorageAddObserver(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef, CFHTTPCookieStorageChangedProcPtr, void*);
void CFHTTPCookieStorageRemoveObserver(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef, CFHTTPCookieStorageChangedProcPtr, void*);

void _CFNetworkSetOverrideSystemProxySettings(CFDictionaryRef);
CFURLCredentialStorageRef CFURLCredentialStorageCreate(CFAllocatorRef);
CFURLCredentialRef CFURLCredentialStorageCopyDefaultCredentialForProtectionSpace(CFURLCredentialStorageRef, CFURLProtectionSpaceRef);
CFURLRequestPriority CFURLRequestGetRequestPriority(CFURLRequestRef);
void _CFURLRequestSetProtocolProperty(CFURLRequestRef, CFStringRef, CFTypeRef);
void CFURLRequestSetRequestPriority(CFURLRequestRef, CFURLRequestPriority);
void CFURLRequestSetShouldPipelineHTTP(CFURLRequestRef, Boolean, Boolean);
void _CFURLRequestSetStorageSession(CFMutableURLRequestRef, CFURLStorageSessionRef);
CFStringRef CFURLResponseCopySuggestedFilename(CFURLResponseRef);
CFHTTPMessageRef CFURLResponseGetHTTPResponse(CFURLResponseRef);
CFStringRef CFURLResponseGetMIMEType(CFURLResponseRef);
CFDictionaryRef _CFURLResponseGetSSLCertificateContext(CFURLResponseRef);
CFURLRef CFURLResponseGetURL(CFURLResponseRef);
void CFURLResponseSetMIMEType(CFURLResponseRef, CFStringRef);
CFHTTPCookieStorageRef _CFURLStorageSessionCopyCookieStorage(CFAllocatorRef, CFURLStorageSessionRef);
CFArrayRef _CFHTTPCookieStorageCopyCookiesForURLWithMainDocumentURL(CFHTTPCookieStorageRef inCookieStorage, CFURLRef inURL, CFURLRef inMainDocumentURL, Boolean sendSecureCookies);
CFStringRef CFURLResponseGetTextEncodingName(CFURLResponseRef);
SInt64 CFURLResponseGetExpectedContentLength(CFURLResponseRef);
CFTypeID CFURLResponseGetTypeID();
Boolean CFURLRequestShouldHandleHTTPCookies(CFURLRequestRef);
CFURLRef CFURLRequestGetURL(CFURLRequestRef);
CFURLResponseRef CFURLResponseCreate(CFAllocatorRef, CFURLRef, CFStringRef mimeType, SInt64 expectedContentLength, CFStringRef textEncodingName, CFURLCacheStoragePolicy);
void CFURLResponseSetExpectedContentLength(CFURLResponseRef, SInt64 length);
CFURLResponseRef CFURLResponseCreateWithHTTPResponse(CFAllocatorRef, CFURLRef, CFHTTPMessageRef, CFURLCacheStoragePolicy);
CFArrayRef CFHTTPCookieStorageCopyCookies(CFHTTPCookieStorageRef);
void CFHTTPCookieStorageSetCookies(CFHTTPCookieStorageRef, CFArrayRef cookies, CFURLRef, CFURLRef mainDocumentURL);
void CFHTTPCookieStorageDeleteCookie(CFHTTPCookieStorageRef, CFHTTPCookieRef);
CFMutableURLRequestRef CFURLRequestCreateMutableCopy(CFAllocatorRef, CFURLRequestRef);
CFStringRef _CFURLCacheCopyCacheDirectory(CFURLCacheRef);
Boolean _CFHostIsDomainTopLevel(CFStringRef domain);
void _CFURLRequestCreateArchiveList(CFAllocatorRef, CFURLRequestRef, CFIndex* version, CFTypeRef** objects, CFIndex* objectCount, CFDictionaryRef* protocolProperties);
CFMutableURLRequestRef _CFURLRequestCreateFromArchiveList(CFAllocatorRef, CFIndex version, CFTypeRef* objects, CFIndex objectCount, CFDictionaryRef protocolProperties);
void CFURLRequestSetProxySettings(CFMutableURLRequestRef, CFDictionaryRef);

#endif // !PLATFORM(WIN)

CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxy;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxyHost;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxyPort;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTAdditionalHeaders;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTResponse;

CFN_EXPORT void _CFHTTPMessageSetResponseURL(CFHTTPMessageRef, CFURLRef);
CFN_EXPORT void _CFHTTPMessageSetResponseProxyURL(CFHTTPMessageRef, CFURLRef);

WTF_EXTERN_C_END

#if defined(__OBJC__) && !USE(APPLE_INTERNAL_SDK)

enum : NSUInteger {
    NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain = 3,
};

@interface NSCachedURLResponse ()
-(id)_initWithCFCachedURLResponse:(CFCachedURLResponseRef)cachedResponse;
-(CFCachedURLResponseRef)_CFCachedURLResponse;
@end

#endif

WTF_EXTERN_C_BEGIN

CFDataRef _CFNetworkCopyATSContext(void);
Boolean _CFNetworkSetATSContext(CFDataRef);

#if PLATFORM(COCOA)
void _CFNetworkResetHSTSHostsSinceDate(CFURLStorageSessionRef, CFDateRef);
#endif

CFDataRef CFHTTPCookieStorageCreateIdentifyingData(CFAllocatorRef inAllocator, CFHTTPCookieStorageRef inStorage);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromIdentifyingData(CFAllocatorRef inAllocator, CFDataRef inData);
CFArrayRef _CFHTTPParsedCookiesWithResponseHeaderFields(CFAllocatorRef inAllocator, CFDictionaryRef headerFields, CFURLRef inURL);

WTF_EXTERN_C_END

#if defined(__OBJC__)

@interface NSHTTPCookie ()
#if PLATFORM(MAC)
+ (NSArray *)_parsedCookiesWithResponseHeaderFields:(NSDictionary *)headerFields forURL:(NSURL *)aURL;
#endif
+ (NSArray *)_cf2nsCookies:(CFArrayRef)cfCookies;
@end

#if !USE(APPLE_INTERNAL_SDK)
@interface NSHTTPCookieStorage ()
- (void)removeCookiesSinceDate:(NSDate *)date;
- (id)_initWithCFHTTPCookieStorage:(CFHTTPCookieStorageRef)cfStorage;
- (CFHTTPCookieStorageRef)_cookieStorage;
- (void)_saveCookies;
- (void)_saveCookies:(dispatch_block_t) completionHandler;
@end
#endif

// FIXME: Move +_setSharedHTTPCookieStorage: into the above section under !USE(APPLE_INTERNAL_SDK) when possible (soon).
@interface NSHTTPCookieStorage ()
+ (void)_setSharedHTTPCookieStorage:(NSHTTPCookieStorage *)storage;
@end

@interface NSURLResponse ()
- (void)_setMIMEType:(NSString *)type;
@end

#endif // defined(__OBJC__)
