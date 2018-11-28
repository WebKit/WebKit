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

#import <WebKit/WKProcessPool.h>
#import <WebKit/WKSecurityOrigin.h>

#if WK_API_ENABLED

@class _WKAutomationSession;
@class _WKDownload;
@class _WKProcessPoolConfiguration;
@protocol _WKAutomationDelegate;
@protocol _WKDownloadDelegate;
@protocol _WKGeolocationCoreLocationProvider;

@interface WKProcessPool ()
- (instancetype)_initWithConfiguration:(_WKProcessPoolConfiguration *)configuration __attribute__((objc_method_family(init))) NS_DESIGNATED_INITIALIZER;
@end

@interface WKProcessPool (WKPrivate)

+ (WKProcessPool *)_sharedProcessPool;

+ (NSArray<WKProcessPool *> *)_allProcessPoolsForTesting WK_API_AVAILABLE(macosx(10.14), ios(12.0));

@property (nonatomic, readonly) _WKProcessPoolConfiguration *_configuration;

- (void)_setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host;
- (void)_setCanHandleHTTPSServerTrustEvaluation:(BOOL)value WK_API_AVAILABLE(macosx(10.11), ios(9.0));
- (void)_setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy;

- (id)_objectForBundleParameter:(NSString *)parameter;
- (void)_setObject:(id <NSCopying, NSSecureCoding>)object forBundleParameter:(NSString *)parameter;
// FIXME: This should be NSDictionary<NSString *, id <NSCopying, NSSecureCoding>>
- (void)_setObjectsForBundleParametersWithDictionary:(NSDictionary *)dictionary WK_API_AVAILABLE(macosx(10.12), ios(10.0));

#if !TARGET_OS_IPHONE
- (void)_resetPluginLoadClientPolicies:(NSDictionary *)policies WK_API_AVAILABLE(macosx(10.13));
@property (nonatomic, readonly, copy) NSDictionary *_pluginLoadClientPolicies WK_API_AVAILABLE(macosx(10.13));
#endif

@property (nonatomic, weak, setter=_setDownloadDelegate:) id <_WKDownloadDelegate> _downloadDelegate;
@property (nonatomic, weak, setter=_setAutomationDelegate:) id <_WKAutomationDelegate> _automationDelegate WK_API_AVAILABLE(macosx(10.12), ios(10.0));

#if TARGET_OS_IPHONE
@property (nonatomic, setter=_setCoreLocationProvider:) id <_WKGeolocationCoreLocationProvider> _coreLocationProvider WK_API_AVAILABLE(ios(11.0));
#endif

+ (NSURL *)_websiteDataURLForContainerWithURL:(NSURL *)containerURL;
+ (NSURL *)_websiteDataURLForContainerWithURL:(NSURL *)containerURL bundleIdentifierIfNotInContainer:(NSString *)bundleIdentifier;

- (void)_warmInitialProcess WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (void)_automationCapabilitiesDidChange WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (void)_setAutomationSession:(_WKAutomationSession *)automationSession WK_API_AVAILABLE(macosx(10.12), ios(10.0));

- (void)_addSupportedPlugin:(NSString *) domain named:(NSString *) name withMimeTypes: (NSSet<NSString *> *) mimeTypes withExtensions: (NSSet<NSString *> *) extensions WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (void)_clearSupportedPlugins WK_API_AVAILABLE(macosx(10.14), ios(12.0));

- (void)_registerURLSchemeAsCanDisplayOnlyIfCanRequest:(NSString *)scheme WK_API_AVAILABLE(macosx(10.14), ios(12.0));

- (_WKDownload *)_downloadURLRequest:(NSURLRequest *)request WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
- (_WKDownload *)_resumeDownloadFromData:(NSData *)resumeData path:(NSString *)path WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

// Test only. Should be called only while no web content processes are running.
- (void)_terminateNetworkProcess;
- (void)_terminateServiceWorkerProcesses WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (void)_disableServiceWorkerProcessTerminationDelay WK_API_AVAILABLE(macosx(10.14), ios(12.0));

// Test only.
- (pid_t)_networkProcessIdentifier WK_API_AVAILABLE(macosx(10.13), ios(11.0));

// Test only.
- (size_t)_webProcessCount WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (BOOL)_hasPrewarmedWebProcess WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
- (size_t)_webProcessCountIgnoringPrewarmed WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (size_t)_pluginProcessCount WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
- (size_t)_serviceWorkerProcessCount WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (void)_syncNetworkProcessCookies WK_API_AVAILABLE(macosx(10.13), ios(11.0));
- (void)_makeNextWebProcessLaunchFailForTesting WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (void)_makeNextNetworkProcessLaunchFailForTesting WK_API_AVAILABLE(macosx(10.14), ios(12.0));
- (NSUInteger)_maximumSuspendedPageCount WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

// Test only. Returns web processes running web pages (does not include web processes running service workers)
- (size_t)_webPageContentProcessCount WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

// Test only. Should be called before any web content processes are launched.
+ (void)_forceGameControllerFramework WK_API_AVAILABLE(macosx(10.13), ios(11.0));

- (void)_preconnectToServer:(NSURL *)serverURL WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

// Test only.
- (void)_setAllowsAnySSLCertificateForServiceWorker:(BOOL)allows WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
- (void)_registerURLSchemeServiceWorkersCanHandle:(NSString *)scheme WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
- (void)_setMaximumNumberOfProcesses:(NSUInteger)value WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
- (void)_getActivePagesOriginsInWebProcessForTesting:(pid_t)pid completionHandler:(void(^)(NSArray<NSString *> *))completionHandler WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
- (BOOL)_networkProcessHasEntitlementForTesting:(NSString *)entitlement WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

@property (nonatomic, getter=_isCookieStoragePartitioningEnabled, setter=_setCookieStoragePartitioningEnabled:) BOOL _cookieStoragePartitioningEnabled WK_API_DEPRECATED("Partitioned cookies are no longer supported", macosx(10.12.3, WK_MAC_TBA), ios(10.3, WK_IOS_TBA));
@property (nonatomic, getter=_isStorageAccessAPIEnabled, setter=_setStorageAccessAPIEnabled:) BOOL _storageAccessAPIEnabled WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

@end

#endif
