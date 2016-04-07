/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
// FIXME: As a workaround for <rdar://problem/18337182>, we conditionally enclose the header
// in an extern "C" linkage block to make it suitable for C++ use.
#ifdef __cplusplus
extern "C" {
#endif

#import <CFNetwork/CFNSURLConnection.h>

#ifdef __cplusplus
}
#endif
#endif // defined(__OBJC__) && PLATFORM(COCOA)

#else // !PLATFORM(WIN) && !USE(APPLE_INTERNAL_SDK)

typedef CF_ENUM(int64_t, _TimingDataOptions)
{
    _TimingDataOptionsEnableW3CNavigationTiming = (1 << 0)
};

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
@interface NSURLRequest ()
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
@property (copy) NSString *_sourceApplicationBundleIdentifier;
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

EXTERN_C void CFURLRequestSetShouldStartSynchronously(CFURLRequestRef, Boolean);

EXTERN_C CFURLCacheRef CFURLCacheCopySharedURLCache();
EXTERN_C void CFURLCacheSetMemoryCapacity(CFURLCacheRef, CFIndex memoryCapacity);
#if PLATFORM(COCOA)
EXTERN_C Boolean _CFNetworkIsKnownHSTSHostWithSession(CFURLRef, CFURLStorageSessionRef);
EXTERN_C void _CFNetworkResetHSTSHostsWithSession(CFURLStorageSessionRef);
#endif

EXTERN_C void CFHTTPCookieStorageDeleteAllCookies(CFHTTPCookieStorageRef);

#if PLATFORM(COCOA)
EXTERN_C CFDataRef _CFCachedURLResponseGetMemMappedData(CFCachedURLResponseRef);
#ifdef __BLOCKS__
EXTERN_C void _CFCachedURLResponseSetBecameFileBackedCallBackBlock(CFCachedURLResponseRef, CFCachedURLResponseCallBackBlock, dispatch_queue_t);
#endif
#endif // PLATFORM(COCOA)

EXTERN_C void CFURLConnectionInvalidateConnectionCache();

EXTERN_C CFStringRef const kCFHTTPCookieLocalFileDomain;
EXTERN_C const CFStringRef kCFURLRequestAllowAllPOSTCaching;

#if !PLATFORM(WIN)
EXTERN_C const CFStringRef _kCFURLConnectionPropertyShouldSniff;
#endif

EXTERN_C CFHTTPCookieStorageRef _CFHTTPCookieStorageGetDefault(CFAllocatorRef);
EXTERN_C void CFHTTPCookieStorageSetCookie(CFHTTPCookieStorageRef, CFHTTPCookieRef);
EXTERN_C void CFHTTPCookieStorageSetCookieAcceptPolicy(CFHTTPCookieStorageRef, CFHTTPCookieStorageAcceptPolicy);
EXTERN_C void _CFNetworkSetOverrideSystemProxySettings(CFDictionaryRef);
EXTERN_C CFURLCredentialStorageRef CFURLCredentialStorageCreate(CFAllocatorRef);
EXTERN_C CFURLCredentialRef CFURLCredentialStorageCopyDefaultCredentialForProtectionSpace(CFURLCredentialStorageRef, CFURLProtectionSpaceRef);
EXTERN_C CFURLRequestPriority CFURLRequestGetRequestPriority(CFURLRequestRef);
EXTERN_C void _CFURLRequestSetProtocolProperty(CFURLRequestRef, CFStringRef, CFTypeRef);
EXTERN_C void CFURLRequestSetRequestPriority(CFURLRequestRef, CFURLRequestPriority);
EXTERN_C void CFURLRequestSetShouldPipelineHTTP(CFURLRequestRef, Boolean, Boolean);
EXTERN_C void _CFURLRequestSetStorageSession(CFMutableURLRequestRef, CFURLStorageSessionRef);
EXTERN_C CFStringRef CFURLResponseCopySuggestedFilename(CFURLResponseRef);
EXTERN_C CFHTTPMessageRef CFURLResponseGetHTTPResponse(CFURLResponseRef);
EXTERN_C CFStringRef CFURLResponseGetMIMEType(CFURLResponseRef);
EXTERN_C CFDictionaryRef _CFURLResponseGetSSLCertificateContext(CFURLResponseRef);
EXTERN_C CFURLRef CFURLResponseGetURL(CFURLResponseRef);
EXTERN_C void CFURLResponseSetMIMEType(CFURLResponseRef, CFStringRef);
EXTERN_C CFHTTPCookieStorageRef _CFURLStorageSessionCopyCookieStorage(CFAllocatorRef, CFURLStorageSessionRef);

// FIXME: We should only forward declare this SPI when building for iOS without the Apple Internal SDK.
// As a workaround for <rdar://problem/19025016>, we must forward declare this SPI regardless of whether
// we are building with the Apple Internal SDK.
#if defined(__OBJC__) && PLATFORM(IOS)
@interface NSURLCache ()
-(id)_initWithMemoryCapacity:(NSUInteger)memoryCapacity diskCapacity:(NSUInteger)diskCapacity relativePath:(NSString *)path;
@end
#endif

#if defined(__OBJC__) && !USE(APPLE_INTERNAL_SDK)
enum : NSUInteger {
    NSHTTPCookieAcceptPolicyExclusivelyFromMainDocumentDomain = 3,
};

@interface NSCachedURLResponse ()
-(id)_initWithCFCachedURLResponse:(CFCachedURLResponseRef)cachedResponse;
-(CFCachedURLResponseRef)_CFCachedURLResponse;
@end
#endif

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
EXTERN_C CFDataRef _CFNetworkCopyATSContext(void);
EXTERN_C Boolean _CFNetworkSetATSContext(CFDataRef);
#endif

#if PLATFORM(COCOA)
EXTERN_C void _CFNetworkResetHSTSHostsSinceDate(CFURLStorageSessionRef, CFDateRef);
#endif

#if TARGET_OS_IPHONE || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
EXTERN_C CFDataRef CFHTTPCookieStorageCreateIdentifyingData(CFAllocatorRef inAllocator, CFHTTPCookieStorageRef inStorage);
EXTERN_C CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromIdentifyingData(CFAllocatorRef inAllocator, CFDataRef inData);
EXTERN_C CFArrayRef _CFHTTPParsedCookiesWithResponseHeaderFields(CFAllocatorRef inAllocator, CFDictionaryRef headerFields, CFURLRef inURL);
#endif

#if defined(__OBJC__)

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
@interface NSHTTPCookie ()
+ (NSArray *)_parsedCookiesWithResponseHeaderFields:(NSDictionary *)headerFields forURL:(NSURL *)aURL;
@end
#endif

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
