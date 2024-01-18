// Copyright © 2020 Apple Inc. All rights reserved.

#import <WebKit/WKFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@class _WKResourceLoadInfo;

// https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/ResourceType
typedef NS_ENUM(NSInteger, _WKWebExtensionWebRequestResourceType) {
    _WKWebExtensionWebRequestResourceTypeMainFrame,
    _WKWebExtensionWebRequestResourceTypeSubframe,
    _WKWebExtensionWebRequestResourceTypeStylesheet,
    _WKWebExtensionWebRequestResourceTypeScript,
    _WKWebExtensionWebRequestResourceTypeImage,
    _WKWebExtensionWebRequestResourceTypeFont,
    _WKWebExtensionWebRequestResourceTypeObject,
    _WKWebExtensionWebRequestResourceTypeXMLHTTPRequest,
    _WKWebExtensionWebRequestResourceTypePing,
    _WKWebExtensionWebRequestResourceTypeCSPReport,
    _WKWebExtensionWebRequestResourceTypeMedia,
    _WKWebExtensionWebRequestResourceTypeWebsocket,
    _WKWebExtensionWebRequestResourceTypeApplicationManifest,
    _WKWebExtensionWebRequestResourceTypeXSLT,
    _WKWebExtensionWebRequestResourceTypeBeacon,
    _WKWebExtensionWebRequestResourceTypeOther,
};

WK_EXTERN _WKWebExtensionWebRequestResourceType _WKWebExtensionWebRequestResourceTypeFromWKResourceLoadInfo(_WKResourceLoadInfo *);

// https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/webRequest/RequestFilter
WK_EXTERN
@interface _WKWebExtensionWebRequestFilter : NSObject

- (nullable instancetype)initWithDictionary:(NSDictionary<NSString *, id> *)dictionary outErrorMessage:(NSString * _Nullable * _Nonnull)outErrorMessage;

- (BOOL)matchesRequestForResourceOfType:(_WKWebExtensionWebRequestResourceType)resourceType URL:(NSURL *)URL tabID:(double)tabID windowID:(double)windowID;

@end

NS_ASSUME_NONNULL_END
