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

#ifndef CFNetworkSPI_h
#define CFNetworkSPI_h

#include "CFNetworkConnectionCacheSPI.h"
#include <CFNetwork/CFNetwork.h>

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
#endif // defined(__OBJC__) && PLATFORM(COCOA)

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
-(instancetype)_initWithMemoryCapacity:(NSUInteger)memoryCapacity diskCapacity:(NSUInteger)diskCapacity relativePath:(NSString *)path;
- (CFURLCacheRef)_CFURLCache;
@end

@interface NSURLRequest ()
+ (NSArray *)allowsSpecificHTTPSCertificateForHost:(NSString *)host;
+ (void)setAllowsSpecificHTTPSCertificate:(NSArray *)allow forHost:(NSString *)host;
- (void)_setProperty:(id)value forKey:(NSString *)key;
@end

@interface NSURLResponse ()
+ (NSURLResponse *)_responseWithCFURLResponse:(CFURLResponseRef)response;
- (CFURLResponseRef)_CFURLResponse;
- (NSDate *)_lastModifiedDate;
@end

@interface NSURLSessionTask (TimingData)
- (NSDictionary *)_timingData;
@end

@interface NSHTTPCookie ()
- (CFHTTPCookieRef)_CFHTTPCookie;
@end

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000)
@interface NSURLSessionConfiguration ()
@property (assign) _TimingDataOptions _timingDataOptions;
@property (copy) NSData *_sourceApplicationAuditTokenData;
@property (nullable, copy) NSString *_sourceApplicationBundleIdentifier;
@property (nullable, copy) NSString *_sourceApplicationSecondaryIdentifier;
@property BOOL _shouldSkipPreferredClientCertificateLookup NS_AVAILABLE(10_10, 8_0);
#if PLATFORM(IOS)
@property (nullable, copy) NSString *_CTDataConnectionServiceType;
#endif
@end
#endif

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

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000)
@interface NSHTTPCookie ()
@property (nullable, readonly, copy) NSString *_storagePartition;
@end

@interface NSHTTPCookieStorage ()
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition completionHandler:(void (^)(NSArray *))completionHandler;
@end

@interface NSURLSessionTask ()
@property (readwrite, copy) NSString *_pathToDownloadTaskFile;
@property (copy) NSString *_storagePartitionIdentifier;
@end
#endif

#endif // defined(__OBJC__)

#endif // !PLATFORM(WIN) && !USE(APPLE_INTERNAL_SDK)

WTF_EXTERN_C_BEGIN

#if !PLATFORM(WIN)
CFURLStorageSessionRef _CFURLStorageSessionCreate(CFAllocatorRef, CFStringRef, CFDictionaryRef);
CFURLCacheRef _CFURLStorageSessionCopyCache(CFAllocatorRef, CFURLStorageSessionRef);

void CFURLRequestSetShouldStartSynchronously(CFURLRequestRef, Boolean);

CFURLCacheRef CFURLCacheCopySharedURLCache();
void CFURLCacheSetMemoryCapacity(CFURLCacheRef, CFIndex memoryCapacity);
CFIndex CFURLCacheMemoryCapacity(CFURLCacheRef);
void CFURLCacheSetDiskCapacity(CFURLCacheRef, CFIndex);

#if PLATFORM(COCOA)
Boolean _CFNetworkIsKnownHSTSHostWithSession(CFURLRef, CFURLStorageSessionRef);
void _CFNetworkResetHSTSHostsWithSession(CFURLStorageSessionRef);
#endif

void CFHTTPCookieStorageDeleteAllCookies(CFHTTPCookieStorageRef);
void _CFHTTPCookieStorageFlushCookieStores();

#if PLATFORM(COCOA)
CFDataRef _CFCachedURLResponseGetMemMappedData(CFCachedURLResponseRef);
#ifdef __BLOCKS__
void _CFCachedURLResponseSetBecameFileBackedCallBackBlock(CFCachedURLResponseRef, CFCachedURLResponseCallBackBlock, dispatch_queue_t);
#endif
#endif // PLATFORM(COCOA)

extern CFStringRef const kCFHTTPCookieLocalFileDomain;
extern const CFStringRef kCFHTTPVersion1_1;
extern const CFStringRef kCFURLRequestAllowAllPOSTCaching;
extern const CFStringRef _kCFURLConnectionPropertyShouldSniff;

CFHTTPCookieStorageRef _CFHTTPCookieStorageGetDefault(CFAllocatorRef);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromFile(CFAllocatorRef, CFURLRef, CFHTTPCookieStorageRef);

void CFHTTPCookieStorageSetCookie(CFHTTPCookieStorageRef, CFHTTPCookieRef);
void CFHTTPCookieStorageSetCookieAcceptPolicy(CFHTTPCookieStorageRef, CFHTTPCookieStorageAcceptPolicy);
CFHTTPCookieStorageAcceptPolicy CFHTTPCookieStorageGetCookieAcceptPolicy(CFHTTPCookieStorageRef);
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
CFURLResponseRef CFURLResponseCreate(CFAllocatorRef, CFURLRef, CFStringRef mimeType, SInt64 expectedContentLength, CFStringRef textEncodingName, CFURLCacheStoragePolicy);
void CFURLResponseSetExpectedContentLength(CFURLResponseRef, SInt64 length);
CFURLResponseRef CFURLResponseCreateWithHTTPResponse(CFAllocatorRef, CFURLRef, CFHTTPMessageRef, CFURLCacheStoragePolicy);

#endif // !PLATFORM(WIN)

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

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
CFDataRef _CFNetworkCopyATSContext(void);
Boolean _CFNetworkSetATSContext(CFDataRef);
#endif

#if PLATFORM(COCOA)
void _CFNetworkResetHSTSHostsSinceDate(CFURLStorageSessionRef, CFDateRef);
#endif

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
CFDataRef CFHTTPCookieStorageCreateIdentifyingData(CFAllocatorRef inAllocator, CFHTTPCookieStorageRef inStorage);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromIdentifyingData(CFAllocatorRef inAllocator, CFDataRef inData);
CFArrayRef _CFHTTPParsedCookiesWithResponseHeaderFields(CFAllocatorRef inAllocator, CFDictionaryRef headerFields, CFURLRef inURL);
#endif

WTF_EXTERN_C_END

#if defined(__OBJC__)

@interface NSHTTPCookie ()
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
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
@end
#endif

// FIXME: Move +_setSharedHTTPCookieStorage: into the above section under !USE(APPLE_INTERNAL_SDK) when possible (soon).
#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
@interface NSHTTPCookieStorage ()
+ (void)_setSharedHTTPCookieStorage:(NSHTTPCookieStorage *)storage;
@end
#endif
#endif // defined(__OBJC__)

#endif // CFNetworkSPI_h
