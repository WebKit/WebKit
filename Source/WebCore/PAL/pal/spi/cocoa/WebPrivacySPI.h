/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#if HAVE(WEB_PRIVACY_FRAMEWORK)
#import <WebPrivacy/WebPrivacy.h>
#else

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, WPResourceType) {
    WPResourceTypeTrackerBlockList = 1,
    WPResourceTypeLinkFilteringData,
    WPResourceTypeTrackerDomains,
    WPResourceTypeTrackerNetworkAddresses,
    WPResourceTypeAllowedLinkFilteringData,
};

typedef NS_ENUM(NSInteger, WPNetworkAddressVersion) {
    WPNetworkAddressVersion4 = 4,
    WPNetworkAddressVersion6 = 6
};

@interface WPNetworkAddressRange : NSObject
@property (nonatomic, readonly) WPNetworkAddressVersion version;
@property (nonatomic, readonly) const struct sockaddr* address;
@property (nonatomic, readonly) NSUInteger netMaskLength;
@property (nonatomic, readonly) NSString *owner;
@property (nonatomic, readonly) NSString *host;
@end

@interface WPResourceRequestOptions : NSObject
@property (nonatomic) BOOL afterUpdates;
@end

@interface WPLinkFilteringRule : NSObject
@property (nonatomic, readonly) NSString *queryParameter;
@property (nonatomic, readonly) NSString *domain;
@end

@interface WPLinkFilteringData : NSObject
@property (nonatomic, readonly) NSArray<WPLinkFilteringRule *> *rules;
@end

@interface WPTrackingDomain  : NSObject
@property (nonatomic, readonly) NSString *host;
@property (nonatomic, readonly) NSString *owner;
@property (nonatomic, readonly) BOOL canBlock;
@end

typedef void (^WPNetworkAddressesCompletionHandler)(NSArray<WPNetworkAddressRange *> *, NSError *);
typedef void (^WPLinkFilteringDataCompletionHandler)(WPLinkFilteringData *, NSError *);
typedef void (^WPTrackingDomainsCompletionHandler)(NSArray<WPTrackingDomain *> *, NSError *);

@interface WPResources : NSObject

+ (instancetype)sharedInstance;

- (void)requestTrackerNetworkAddresses:(WPResourceRequestOptions *)options completionHandler:(WPNetworkAddressesCompletionHandler)completion;
- (void)requestLinkFilteringData:(WPResourceRequestOptions *)options completionHandler:(WPLinkFilteringDataCompletionHandler)completion;
- (void)requestAllowedLinkFilteringData:(WPResourceRequestOptions *)options completionHandler:(WPLinkFilteringDataCompletionHandler)completion;
- (void)requestTrackerDomainNamesData:(WPResourceRequestOptions *)options completionHandler:(WPTrackingDomainsCompletionHandler)completion;

@end

#endif // !HAVE(WEB_PRIVACY_FRAMEWORK)

#if !defined(HAS_WEB_PRIVACY_STORAGE_ACCESS_PROMPT_QUIRK_CLASS)
constexpr NSInteger WPResourceTypeStorageAccessPromptQuirksData = 7;

@interface WPStorageAccessPromptQuirk : NSObject
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSDictionary<NSString *, NSArray<NSString *> *> *domainPairings;
@end

@interface WPStorageAccessPromptQuirksData : NSObject
@property (nonatomic, readonly) NSArray<WPStorageAccessPromptQuirk *> *quirks;
@end

typedef void (^WPStorageAccessPromptQuirksDataCompletionHandler)(WPStorageAccessPromptQuirksData *, NSError *);

@interface WPResources (Staging_119342418)
- (void)requestStorageAccessPromptQuirksData:(WPResourceRequestOptions *)options completionHandler:(WPStorageAccessPromptQuirksDataCompletionHandler)completion;
@end
#endif

#if !defined(HAS_WEB_PRIVACY_STORAGE_ACCESS_USER_AGENT_STRING_CLASS)
constexpr NSInteger WPResourceTypeStorageAccessUserAgentStringQuirksData = 6;
@interface WPStorageAccessUserAgentStringQuirk : NSObject
@property (nonatomic, readonly) NSString *domain;
@property (nonatomic, readonly) NSString *userAgentString;
@end

@interface WPStorageAccessUserAgentStringQuirksData : NSObject
@property (nonatomic, readonly) NSArray<WPStorageAccessUserAgentStringQuirk *> *quirks;
@end

typedef void (^WPStorageAccessUserAgentStringQuirksDataCompletionHandler)(WPStorageAccessUserAgentStringQuirksData *, NSError *);

@interface WPStorageAccessUserAgentStringQuirk (Staging_119342418)
- (void)requestStorageAccessUserAgentStringQuirksData:(WPResourceRequestOptions *)options completionHandler:(WPStorageAccessUserAgentStringQuirksDataCompletionHandler)completion;
@end
#endif

WTF_EXTERN_C_BEGIN

extern NSString *const WPNotificationUserInfoResourceTypeKey;
extern NSNotificationName const WPResourceDataChangedNotificationName;

WTF_EXTERN_C_END

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
