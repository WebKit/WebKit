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

#if PLATFORM(WIN) || USE(APPLE_INTERNAL_SDK)

#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CFNetwork/CFHTTPStream.h>
#include <CFNetwork/CFProxySupportPriv.h>
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLConnectionPriv.h>
#include <CFNetwork/CFURLCredentialStorage.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <CFNetwork/CFURLRequestPriv.h>
#include <CFNetwork/CFURLResponsePriv.h>
#include <CFNetwork/CFURLStorageSession.h>

#if PLATFORM(WIN)

WTF_EXTERN_C_BEGIN

CFN_EXPORT CFStringRef _CFNetworkErrorGetLocalizedDescription(CFIndex);

extern const CFStringRef _kCFWindowsSSLLocalCert;
extern const CFStringRef _kCFStreamPropertyWindowsSSLCertInfo;
extern const CFStringRef _kCFWindowsSSLPeerCert;

WTF_EXTERN_C_END

#else // !PLATFORM(WIN)
#include <CFNetwork/CFSocketStreamPriv.h>
#include <nw/private.h>
#endif // PLATFORM(WIN)

// FIXME: Remove the defined(__OBJC__)-guard once we fix <rdar://problem/19033610>.
#if defined(__OBJC__) && PLATFORM(COCOA)
#import <CFNetwork/CFNSURLConnection.h>
#endif

#else // !PLATFORM(WIN) && !USE(APPLE_INTERNAL_SDK)

#if HAVE(LOGGING_PRIVACY_LEVEL)
typedef enum {
    nw_context_privacy_level_public = 1,
    nw_context_privacy_level_private = 2,
    nw_context_privacy_level_sensitive = 3,
    nw_context_privacy_level_silent = 4,
} nw_context_privacy_level_t;

#ifndef NW_CONTEXT_HAS_PRIVACY_LEVEL_SILENT
#define NW_CONTEXT_HAS_PRIVACY_LEVEL_SILENT    1
#endif
#endif // HAVE(LOGGING_PRIVACY_LEVEL)

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

CF_ENUM(CFHTTPCookieStorageAcceptPolicy)
{
    CFHTTPCookieStorageAcceptPolicyAlways = 0,
    CFHTTPCookieStorageAcceptPolicyNever = 1,
    CFHTTPCookieStorageAcceptPolicyOnlyFromMainDocumentDomain = 2,
    CFHTTPCookieStorageAcceptPolicyExclusivelyFromMainDocumentDomain = 3,
};

#ifdef __BLOCKS__
typedef void (^CFCachedURLResponseCallBackBlock)(CFCachedURLResponseRef);
#endif

#if defined(__OBJC__)

@interface NSURLSessionTask ()
@property (readonly, retain) NSURLSessionTaskMetrics* _incompleteTaskMetrics;
@end

@interface NSURLCache ()
- (CFURLCacheRef)_CFURLCache;
@end

@interface NSCachedURLResponse ()
- (id)_initWithCFCachedURLResponse:(CFCachedURLResponseRef)cachedResponse;
- (CFCachedURLResponseRef)_CFCachedURLResponse;
@end

@interface NSHTTPCookie ()
- (CFHTTPCookieRef)_CFHTTPCookie;
+ (CFArrayRef __nullable)_ns2cfCookies:(NSArray * __nullable)nsCookies CF_RETURNS_RETAINED;
- (CFHTTPCookieRef __nullable)_GetInternalCFHTTPCookie;
@property (nullable, readonly, copy) NSString *_storagePartition;
@end

@interface NSHTTPCookieStorage ()
- (id)_initWithIdentifier:(NSString *)identifier private:(bool)isPrivate;
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition completionHandler:(void (^)(NSArray *))completionHandler;
- (void)_getCookiesForURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL partition:(NSString *)partition policyProperties:(NSDictionary*)props completionHandler:(void (NS_NOESCAPE ^)(NSArray *))completionHandler;
- (void)_setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL policyProperties:(NSDictionary*) props;
- (void)removeCookiesSinceDate:(NSDate *)date;
- (id)_initWithCFHTTPCookieStorage:(CFHTTPCookieStorageRef)cfStorage;
- (CFHTTPCookieStorageRef)_cookieStorage;
- (void)_saveCookies;
#if HAVE(FOUNDATION_WITH_SAVE_COOKIES_WITH_COMPLETION_HANDLER)
- (void)_saveCookies:(dispatch_block_t) completionHandler;
#endif
#if HAVE(CFNETWORK_OVERRIDE_SESSION_COOKIE_ACCEPT_POLICY)
@property (nonatomic, readwrite) BOOL _overrideSessionCookieAcceptPolicy;
#endif
@end

@interface NSURLConnection ()
- (id)_initWithRequest:(NSURLRequest *)request delegate:(id)delegate usesCache:(BOOL)usesCacheFlag maxContentLength:(long long)maxContentLength startImmediately:(BOOL)startImmediately connectionProperties:(NSDictionary *)connectionProperties;
@end

@interface NSMutableURLRequest ()
- (void)setContentDispositionEncodingFallbackArray:(NSArray *)theEncodingFallbackArray;
- (void)setBoundInterfaceIdentifier:(NSString *)identifier;
- (void)_setPreventHSTSStorage:(BOOL)preventHSTSStorage;
- (void)_setIgnoreHSTS:(BOOL)ignoreHSTS;
@end

@interface NSURLProtocol ()
+ (Class)_protocolClassForRequest:(NSURLRequest *)request;
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
- (BOOL)_schemeWasUpgradedDueToDynamicHSTS;
- (BOOL)_preventHSTSStorage;
- (BOOL)_ignoreHSTS;
@end

@interface NSURLResponse ()
+ (NSURLResponse *)_responseWithCFURLResponse:(CFURLResponseRef)response;
- (CFURLResponseRef)_CFURLResponse;
- (NSDate *)_lastModifiedDate;
@end

#if PLATFORM(WATCHOS)
typedef NS_ENUM(NSInteger, NSURLSessionCompanionProxyPreference) {
    NSURLSessionCompanionProxyPreferenceDefault = 0,
    NSURLSessionCompanionProxyPreferencePreferCompanion,
    NSURLSessionCompanionProxyPreferencePreferDirectToCloud,
};
#endif

@interface NSURLSessionConfiguration ()
@property (assign) _TimingDataOptions _timingDataOptions;
@property (copy) NSData *_sourceApplicationAuditTokenData;
@property (nullable, copy) NSString *_sourceApplicationBundleIdentifier;
@property (nullable, copy) NSString *_sourceApplicationSecondaryIdentifier;
@property BOOL _shouldSkipPreferredClientCertificateLookup;
@property BOOL _preventsSystemHTTPProxyAuthentication;
@property BOOL _requiresSecureHTTPSProxyConnection;
#if PLATFORM(IOS_FAMILY)
@property (nullable, copy) NSString *_CTDataConnectionServiceType;
#endif
@property (nullable, copy) NSSet *_suppressedAutoAddedHTTPHeaders;
#if PLATFORM(WATCHOS)
@property NSURLSessionCompanionProxyPreference _companionProxyPreference;
#endif
#if HAVE(APP_SSO)
@property BOOL _preventsAppSSO;
#endif
#if HAVE(ALLOWS_SENSITIVE_LOGGING)
@property BOOL _allowsSensitiveLogging;
#endif
#if HAVE(LOGGING_PRIVACY_LEVEL)
@property nw_context_privacy_level_t _loggingPrivacyLevel;
#endif
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
@property (nullable, retain) _NSHTTPAlternativeServicesStorage *_alternativeServicesStorage;
@property (readonly, assign) BOOL _allowsHTTP3;
#endif
@end

@interface NSURLSessionTask ()
- (NSDictionary *)_timingData;
@property (readwrite, copy) NSString *_pathToDownloadTaskFile;
@property (copy) NSString *_storagePartitionIdentifier;
#if HAVE(FOUNDATION_WITH_SAME_SITE_COOKIE_SUPPORT)
@property (nullable, readwrite, retain) NSURL *_siteForCookies;
@property (readwrite) BOOL _isTopLevelNavigation;
#endif
#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
@property (nonatomic, assign) BOOL _preconnect;
#endif
@end

@interface NSURLSessionTaskTransactionMetrics ()
@property (copy, readonly) NSString* _remoteAddressAndPort;
@property (copy, readonly) NSUUID* _connectionIdentifier;
@property (assign, readonly) NSInteger _requestHeaderBytesSent;
@property (assign, readonly) NSInteger _responseHeaderBytesReceived;
@property (assign, readonly) int64_t _responseBodyBytesReceived;
@property (assign, readonly) int64_t _responseBodyBytesDecoded;
@end

#if HAVE(CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER)
@interface NSURLSessionTaskTransactionMetrics ()
@property (assign) SSLProtocol _negotiatedTLSProtocol;
@property (assign) SSLCipherSuite _negotiatedTLSCipher;
@end
#endif

@interface NSURLSession (SPI)
#if HAVE(CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE)
+ (void)_strictTrustEvaluate:(NSURLAuthenticationChallenge *)challenge queue:(dispatch_queue_t)queue completionHandler:(void (^)(NSURLAuthenticationChallenge *challenge, OSStatus trustResult))cb;
#endif
#if HAVE(APP_SSO)
+ (void)_disableAppSSO;
#endif
@end

#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)

@interface _NSHTTPAlternativeServiceEntry : NSObject <NSCopying>
@property (readwrite, nonatomic, retain) NSString *host;
@property (readwrite, nonatomic, assign) NSInteger port;
@end

@interface _NSHTTPAlternativeServicesFilter : NSObject <NSCopying>
+ (instancetype)emptyFilter;
@end

@interface _NSHTTPAlternativeServicesStorage : NSObject
@property (readonly, nonatomic) NSURL *path;
+ (instancetype)sharedPersistentStore;
- (instancetype)initPersistentStoreWithURL:(nullable NSURL *)path;
- (NSArray<_NSHTTPAlternativeServiceEntry *> *)HTTPServiceEntriesWithFilter:(_NSHTTPAlternativeServicesFilter *)filter;
- (void)removeHTTPAlternativeServiceEntriesWithRegistrableDomain:(NSString *)domain;
- (void)removeHTTPAlternativeServiceEntriesCreatedAfterDate:(NSDate *)date;
@end

#endif // HAVE(CFNETWORK_ALTERNATIVE_SERVICE)

extern NSString * const NSURLAuthenticationMethodOAuth;

#endif // defined(__OBJC__)

#endif // PLATFORM(WIN) || USE(APPLE_INTERNAL_SDK)

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
extern const CFStringRef kCFStreamSocketSecurityLevelTLSv1_2;
extern const CFStringRef kCFURLRequestContentDecoderSkipURLCheck;

CFHTTPCookieStorageRef _CFHTTPCookieStorageGetDefault(CFAllocatorRef);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromFile(CFAllocatorRef, CFURLRef, CFHTTPCookieStorageRef);
void CFHTTPCookieStorageScheduleWithRunLoop(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef);
void CFHTTPCookieStorageSetCookie(CFHTTPCookieStorageRef, CFHTTPCookieRef);
void CFHTTPCookieStorageSetCookieAcceptPolicy(CFHTTPCookieStorageRef, CFHTTPCookieStorageAcceptPolicy);
CFHTTPCookieStorageAcceptPolicy CFHTTPCookieStorageGetCookieAcceptPolicy(CFHTTPCookieStorageRef);

typedef void (*CFHTTPCookieStorageChangedProcPtr)(CFHTTPCookieStorageRef, void*);
void CFHTTPCookieStorageAddObserver(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef, CFHTTPCookieStorageChangedProcPtr, void*);
void CFHTTPCookieStorageRemoveObserver(CFHTTPCookieStorageRef, CFRunLoopRef, CFStringRef, CFHTTPCookieStorageChangedProcPtr, void*);

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

#if HAVE(HSTS_STORAGE_PATH)
void _CFNetworkSetHSTSStoragePath(CFStringRef);
#endif

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

#endif

WTF_EXTERN_C_BEGIN

CFDataRef _CFNetworkCopyATSContext(void);
Boolean _CFNetworkSetATSContext(CFDataRef);

#if PLATFORM(COCOA)
extern const CFStringRef _kCFNetworkHSTSPreloaded;
CFDictionaryRef _CFNetworkCopyHSTSPolicies(CFURLStorageSessionRef);
void _CFNetworkResetHSTS(CFURLRef, CFURLStorageSessionRef);
void _CFNetworkResetHSTSHostsSinceDate(CFURLStorageSessionRef, CFDateRef);
Boolean _CFNetworkIsKnownHSTSHostWithSession(CFURLRef, CFURLStorageSessionRef);
void _CFNetworkResetHSTSHostsWithSession(CFURLStorageSessionRef);
#endif

CFDataRef CFHTTPCookieStorageCreateIdentifyingData(CFAllocatorRef inAllocator, CFHTTPCookieStorageRef inStorage);
CFHTTPCookieStorageRef CFHTTPCookieStorageCreateFromIdentifyingData(CFAllocatorRef inAllocator, CFDataRef inData);
CFArrayRef _CFHTTPParsedCookiesWithResponseHeaderFields(CFAllocatorRef inAllocator, CFDictionaryRef headerFields, CFURLRef inURL);

WTF_EXTERN_C_END

#if defined(__OBJC__)

// FIXME: Move these declarations the above section under !USE(APPLE_INTERNAL_SDK) when possible so that
// Apple internal SDK builds use headers instead.

@interface NSHTTPCookie ()
#if PLATFORM(MAC)
+ (NSArray *)_parsedCookiesWithResponseHeaderFields:(NSDictionary *)headerFields forURL:(NSURL *)aURL;
#endif
+ (NSArray *)_cf2nsCookies:(CFArrayRef)cfCookies;
@end

@interface NSHTTPCookieStorage ()
+ (void)_setSharedHTTPCookieStorage:(NSHTTPCookieStorage *)storage;
- (void)_setSubscribedDomainsForCookieChanges:(NSSet<NSString*>* __nullable)domainList;
- (NSArray* __nullable)_getCookiesForDomain:(NSString*)domain;
- (void)_setCookiesChangedHandler:(void(^__nullable)(NSArray<NSHTTPCookie*>* addedCookies, NSString* domainForChangedCookie))cookiesChangedHandler onQueue:(dispatch_queue_t __nullable)queue;
- (void)_setCookiesRemovedHandler:(void(^__nullable)(NSArray<NSHTTPCookie*>* __nullable removedCookies, NSString* __nullable domainForRemovedCookies, bool removeAllCookies))cookiesRemovedHandler onQueue:(dispatch_queue_t __nullable)queue;
@end

@interface __NSCFLocalDownloadFile : NSObject
@end
@interface __NSCFLocalDownloadFile ()
@property (readwrite, assign) BOOL skipUnlink;
@end

@interface NSURLSessionDownloadTask ()
- (__NSCFLocalDownloadFile *)downloadFile;
@end

@interface NSURLResponse ()
- (void)_setMIMEType:(NSString *)type;
@end

@interface NSURLSessionConfiguration ()
// FIXME: Remove this once rdar://problem/40650244 is in a build.
@property (copy) NSDictionary *_socketStreamProperties;
@end

@interface NSURLSessionTask ()
- (void)_setExplicitCookieStorage:(CFHTTPCookieStorageRef)storage;
@property (readonly) SSLProtocol _TLSNegotiatedProtocolVersion;
@end

#endif // defined(__OBJC__)
