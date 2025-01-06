/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "CoreIPCBoolean.h"
#import "CoreIPCData.h"
#import "CoreIPCDate.h"
#import "CoreIPCNumber.h"
#import "CoreIPCPlistArray.h"
#import "CoreIPCPlistDictionary.h"
#import "CoreIPCString.h"
#import "CoreIPCURL.h"

#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Vector.h>

OBJC_CLASS NSURLRequest;

namespace WebKit {

enum class NSURLRequestCachePolicy : uint8_t {
    UseProtocolCachePolicy,
    ReloadIgnoringLocalCacheData,
    ReturnCacheDataElseLoad,
    ReturnCacheDataDontLoad,
    ReloadIgnoringLocalAndRemoteCacheData,
    ReloadRevalidatingCacheData,
};

enum class NSURLRequestNetworkServiceType : uint8_t {
    Default,
    OpaqueValue0,
    OpaqueValue1,
    Background,
    OpaqueValue2,
    OpaqueValue3,
    OpaqueValue4,
    OpaqueValue5,
    AVStreaming,
    OpaqueValue6,
    OpaqueValue7,
    OpaqueValue8
};

enum class NSURLRequestAllowedProtocolTypes : uint8_t {
    OpaqueValue1 = (1 << 0),
    OpaqueValue2 = (1 << 1),
    OpaqueValue3 = (1 << 2),
    OpaqueValue4 = (1 << 3),
    OpaqueValue5 = (1 << 4),
    OpaqueValue6 = (1 << 5),
    OpaqueValue7 = (1 << 6),
    OpaqueValue8 = (1 << 7),
};

enum class NSURLRequestAttribution : bool {
    Developer,
    User
};

enum class NSURLRequestFlags : int16_t {
    ShouldHandleCookies = (1 << 1),
    NetworkServiceType = (1 << 2),
    AllowsCellular = (1 << 3),
    PreventsIdleSleep = (1 << 4),
    ShouldPipeline = (1 << 6),
    CachePolicy = (1 << 7),
    Timeout = (1 << 8),
    ProxyDict = (1 << 9),
    SSLProperties = (1 << 10),
    ShouldStartSynchronously = (1 << 12)
};

struct CoreIPCNSURLRequestData {

    using BodyParts = std::variant<CoreIPCString, CoreIPCData>;
    using HeaderField = std::pair<String, std::variant<String, Vector<String>>>;

    std::optional<CoreIPCPlistDictionary> protocolProperties;
    bool isMutable { false };
    CoreIPCURL url;
    double timeout { 0 };
    NSURLRequestCachePolicy cachePolicy = NSURLRequestCachePolicy::UseProtocolCachePolicy;
    std::optional<CoreIPCURL> mainDocumentURL;
    bool shouldHandleHTTPCookies { false };
    OptionSet<NSURLRequestFlags> explicitFlags;
    bool allowCellular { false };
    bool preventsIdleSystemSleep { false };
    double timeWindowDelay { 0 };
    double timeWindowDuration { 0 };
    NSURLRequestNetworkServiceType networkServiceType = NSURLRequestNetworkServiceType::Default;
    int requestPriority { 0 };
    std::optional<bool> isHTTP;
    std::optional<CoreIPCString> httpMethod;
    std::optional<Vector<HeaderField>> headerFields;
    std::optional<CoreIPCData> body;
    std::optional<Vector<BodyParts>> bodyParts;

    double startTimeoutTime { 0 };
    bool requiresShortConnectionTimeout { false };
    double payloadTransmissionTimeout { 0 };
    OptionSet<NSURLRequestAllowedProtocolTypes> allowedProtocolTypes;
    std::optional<CoreIPCString> boundInterfaceIdentifier;
    std::optional<bool> allowsExpensiveNetworkAccess;
    std::optional<bool> allowsConstrainedNetworkAccess;
    std::optional<bool> allowsUCA;
    bool assumesHTTP3Capable { false };
    NSURLRequestAttribution attribution = NSURLRequestAttribution::Developer;
    bool knownTracker { false };
    std::optional<CoreIPCString> trackerContext;
    bool privacyProxyFailClosed { false };
    bool privacyProxyFailClosedForUnreachableNonMainHosts { false };
    bool privacyProxyFailClosedForUnreachableHosts { false };
    std::optional<bool> requiresDNSSECValidation;
    bool allowsPersistentDNS { false };
    bool prohibitPrivacyProxy { false };
    bool allowPrivateAccessTokensForThirdParty { false };
    bool useEnhancedPrivacyMode { false };
    bool blockTrackers { false };
    bool failInsecureLoadWithHTTPSDNSRecord { false };
    bool isWebSearchContent { false };
    bool allowOnlyPartitionedCookies { false };
    std::optional<Vector<CoreIPCNumber>> contentDispositionEncodingFallbackArray;
};

class CoreIPCNSURLRequest {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCNSURLRequest);
public:
    CoreIPCNSURLRequest(NSURLRequest *);
    CoreIPCNSURLRequest(CoreIPCNSURLRequestData&&);
    CoreIPCNSURLRequest(const RetainPtr<NSURLRequest>&);

    RetainPtr<id> toID() const;
private:
    friend struct IPC::ArgumentCoder<CoreIPCNSURLRequest, void>;
    CoreIPCNSURLRequestData m_data;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
