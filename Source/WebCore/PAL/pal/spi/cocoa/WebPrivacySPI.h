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

DECLARE_SYSTEM_HEADER

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
@property (nonatomic, readonly) NSString *path;
@end

@interface WPLinkFilteringData : NSObject
@property (nonatomic, readonly) NSArray<WPLinkFilteringRule *> *rules;
@end

@interface WPTrackingDomain : NSObject
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
@property (nonatomic, readonly) NSDictionary<NSString *, NSArray<NSString *> *> *quirkDomains;
@property (nonatomic, readonly) NSArray<NSString *> *triggerPages;
@end

@interface WPStorageAccessPromptQuirksData : NSObject
@property (nonatomic, readonly) NSArray<WPStorageAccessPromptQuirk *> *quirks;
@end

typedef void (^WPStorageAccessPromptQuirksDataCompletionHandler)(WPStorageAccessPromptQuirksData *, NSError *);

@interface WPResources (Staging_119342418_PromptQuirks)
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

@interface WPResources (Staging_119342418_UAQuirks)
- (void)requestStorageAccessUserAgentStringQuirksData:(WPResourceRequestOptions *)options completionHandler:(WPStorageAccessUserAgentStringQuirksDataCompletionHandler)completion;
@end
#endif

#if !defined(HAS_WEB_PRIVACY_LINK_FILTERING_RULE_PATH) && HAVE(WEB_PRIVACY_FRAMEWORK)
@interface WPLinkFilteringRule (Staging_119590894)
@property (nonatomic, readonly) NSString *path;
@end
#endif

#if !defined(HAS_WEB_PRIVACY_RESTRICTED_OPENER_DOMAIN_CLASS)
constexpr NSInteger WPResourceTypeRestrictedOpenerDomains = 8;

typedef NS_ENUM(NSInteger, WPRestrictedOpenerType) {
    WPRestrictedOpenerTypeNoOpener = 1,
    WPRestrictedOpenerTypePostMessageAndClose,
};

@interface WPRestrictedOpenerDomain : NSObject
@property (nonatomic, readonly) NSString *domain;
@property (nonatomic, readonly) WPRestrictedOpenerType openerType;
@end

typedef void (^WPRestrictedOpenerDomainsCompletionHandler)(NSArray<WPRestrictedOpenerDomain *> *, NSError *);

@interface WPResources (Staging_118208263)
- (void)requestRestrictedOpenerDomains:(WPResourceRequestOptions *)options completionHandler:(WPRestrictedOpenerDomainsCompletionHandler)completion;
@end
#endif

#if !defined(HAS_WEB_PRIVACY_STORAGE_ACCESS_PROMPT_TRIGGER) && HAVE(WEB_PRIVACY_FRAMEWORK)
@interface WPStorageAccessPromptQuirk (Staging_124689085)
@property (nonatomic, readonly) NSDictionary<NSString *, NSArray<NSString *> *> *quirkDomains;
@property (nonatomic, readonly) NSArray<NSString *> *triggerPages;
@end
#endif

@class WKContentRuleList;
@class WKContentRuleListStore;

typedef void (^WKWPResourcesPrepareCompletionHandler)(WKContentRuleList *, bool, NSError *);

@interface WPResources (Staging_141646051)
- (void)prepareResourceMonitorRulesForStore:(WKContentRuleListStore *)store completionHandler:(WKWPResourcesPrepareCompletionHandler)completionHandler;
@end

typedef void (^WKWPResourcesGetSourceCompletionHandler)(NSString *, NSError *);

@interface WPResources (Staging_146076707)
- (void)requestResourceMonitorRulesSource:(WPResourceRequestOptions *)options completionHandler:(WKWPResourcesGetSourceCompletionHandler)completion;
@end

#if !__has_include(<WebPrivacy/WPFingerprintingScript.h>)

#define WPResourceTypeFingerprintingScripts ((WPResourceType)9)

// Staging declaration for macOS downlevels.
@interface WPFingerprintingScript : NSObject
@property (nonatomic, readonly) NSString *host;
@property (nonatomic, readonly, getter=isFirstParty) BOOL firstParty;
@property (nonatomic, readonly, getter=isTopDomain) BOOL topDomain;
@end

using WPFingerprintingScriptCompletionHandler = void (^)(NSArray<WPFingerprintingScript *> *, NSError *);

@interface WPResources (Staging_135619791)
- (void)requestFingerprintingScripts:(WPResourceRequestOptions *)options completionHandler:(WPFingerprintingScriptCompletionHandler)completion;
@end

#endif // !__has_include(<WebPrivacy/WPFingerprintingScript.h>)

#if !defined(WP_SUPPORTS_SCRIPT_ACCESS_CATEGORY)

typedef NS_OPTIONS(NSUInteger, WPScriptAccessCategories) {
    WPScriptAccessCategoryNone                  = 0,
    WPScriptAccessCategoryAudio                 = 1 << 0,
    WPScriptAccessCategoryCanvas                = 1 << 1,
    WPScriptAccessCategoryCookies               = 1 << 2,
    WPScriptAccessCategoryHardwareConcurrency   = 1 << 3,
    WPScriptAccessCategoryLocalStorage          = 1 << 4,
    WPScriptAccessCategoryPayments              = 1 << 5,
    WPScriptAccessCategoryQueryParameters       = 1 << 6,
    WPScriptAccessCategoryReferrer              = 1 << 7,
    WPScriptAccessCategoryScreenOrViewport      = 1 << 8,
    WPScriptAccessCategorySpeech                = 1 << 9,
    WPScriptAccessCategoryFormControls          = 1 << 10,
};

@interface WPFingerprintingScript (Staging_155749047)
@property (nonatomic, readonly) WPScriptAccessCategories allowedCategories;
@end

#endif // !defined(WP_SUPPORTS_SCRIPT_ACCESS_CATEGORY)

WTF_EXTERN_C_BEGIN

extern NSString *const WPNotificationUserInfoResourceTypeKey;
extern NSNotificationName const WPResourceDataChangedNotificationName;

WTF_EXTERN_C_END

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
