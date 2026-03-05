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

#import "config.h"
#import "ProtectionSpaceCocoa.h"

#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RuntimeApplicationChecks.h>

namespace WebCore {

static ProtectionSpace::ServerType type(NSURLProtectionSpace *space)
{
    if ([space isProxy]) {
        RetainPtr<NSString> proxyType = space.proxyType;
        if ([proxyType isEqualToString:NSURLProtectionSpaceHTTPProxy])
            return ProtectionSpace::ServerType::ProxyHTTP;
        if ([proxyType isEqualToString:NSURLProtectionSpaceHTTPSProxy])
            return ProtectionSpace::ServerType::ProxyHTTPS;
// FIXME: rdar://144814079
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([proxyType isEqualToString:NSURLProtectionSpaceFTPProxy])
            return ProtectionSpace::ServerType::ProxyFTP;
ALLOW_DEPRECATED_DECLARATIONS_END
        if ([proxyType isEqualToString:NSURLProtectionSpaceSOCKSProxy])
            return ProtectionSpace::ServerType::ProxySOCKS;

        ASSERT_NOT_REACHED();
        return ProtectionSpace::ServerType::ProxyHTTP;
    }

    RetainPtr<NSString> protocol = space.protocol;
    if ([protocol caseInsensitiveCompare:@"http"] == NSOrderedSame)
        return ProtectionSpace::ServerType::HTTP;
    if ([protocol caseInsensitiveCompare:@"https"] == NSOrderedSame)
        return ProtectionSpace::ServerType::HTTPS;
    if ([protocol caseInsensitiveCompare:@"ftp"] == NSOrderedSame)
        return ProtectionSpace::ServerType::FTP;
    if ([protocol caseInsensitiveCompare:@"ftps"] == NSOrderedSame)
        return ProtectionSpace::ServerType::FTPS;

    ASSERT_NOT_REACHED();
    return ProtectionSpace::ServerType::HTTP;
}

static ProtectionSpace::AuthenticationScheme scheme(NSURLProtectionSpace *space)
{
    RetainPtr<NSString> method = space.authenticationMethod;
    if ([method isEqualToString:NSURLAuthenticationMethodDefault])
        return ProtectionSpace::AuthenticationScheme::Default;
    if ([method isEqualToString:NSURLAuthenticationMethodHTTPBasic])
        return ProtectionSpace::AuthenticationScheme::HTTPBasic;
    if ([method isEqualToString:NSURLAuthenticationMethodHTTPDigest])
        return ProtectionSpace::AuthenticationScheme::HTTPDigest;
    if ([method isEqualToString:NSURLAuthenticationMethodHTMLForm])
        return ProtectionSpace::AuthenticationScheme::HTMLForm;
    if ([method isEqualToString:NSURLAuthenticationMethodNTLM])
        return ProtectionSpace::AuthenticationScheme::NTLM;
    if ([method isEqualToString:NSURLAuthenticationMethodNegotiate])
        return ProtectionSpace::AuthenticationScheme::Negotiate;
// FIXME: rdar://144814079
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    if ([method isEqualToString:NSURLAuthenticationMethodClientCertificate])
        return ProtectionSpace::AuthenticationScheme::ClientCertificateRequested;
    if ([method isEqualToString:NSURLAuthenticationMethodServerTrust])
        return ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested;
#endif
    if ([method isEqualToString:NSURLAuthenticationMethodOAuth])
        return ProtectionSpace::AuthenticationScheme::OAuth;

    ASSERT_NOT_REACHED();
    return ProtectionSpace::AuthenticationScheme::Default;
}

ProtectionSpace::ProtectionSpace(NSURLProtectionSpace *space)
    : ProtectionSpace(space.host, space.port, type(space), space.realm, scheme(space))
{
    lazyInitialize(m_nsSpace, RetainPtr { space });
}

ProtectionSpace::ProtectionSpace(const String& host, int port, ServerType serverType, const String& realm, AuthenticationScheme authenticationScheme, std::optional<PlatformData> platformData)
    : ProtectionSpaceBase(host, port, serverType, realm, authenticationScheme)
{
    if (platformData)
        lazyInitialize(m_nsSpace, RetainPtr { platformData->nsSpace });
}

NSURLProtectionSpace *ProtectionSpace::nsSpace() const
{
    if (m_nsSpace)
        return m_nsSpace.get();

    RetainPtr<NSString> proxyType;
    RetainPtr<NSString> protocol;
    switch (serverType()) {
    case ProtectionSpace::ServerType::HTTP:
        protocol = @"http";
        break;
    case ProtectionSpace::ServerType::HTTPS:
        protocol = @"https";
        break;
    case ProtectionSpace::ServerType::FTP:
        protocol = @"ftp";
        break;
    case ProtectionSpace::ServerType::FTPS:
        protocol = @"ftps";
        break;
    case ProtectionSpace::ServerType::ProxyHTTP:
        proxyType = NSURLProtectionSpaceHTTPProxy;
        break;
    case ProtectionSpace::ServerType::ProxyHTTPS:
        proxyType = NSURLProtectionSpaceHTTPSProxy;
        break;
// FIXME: rdar://144814079
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    case ProtectionSpace::ServerType::ProxyFTP:
        proxyType = NSURLProtectionSpaceFTPProxy;
        break;
ALLOW_DEPRECATED_DECLARATIONS_END
    case ProtectionSpace::ServerType::ProxySOCKS:
        proxyType = NSURLProtectionSpaceSOCKSProxy;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
  
    NSString *method = nil;
    switch (authenticationScheme()) {
    case ProtectionSpace::AuthenticationScheme::Default:
        method = NSURLAuthenticationMethodDefault;
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPBasic:
        method = NSURLAuthenticationMethodHTTPBasic;
        break;
    case ProtectionSpace::AuthenticationScheme::HTTPDigest:
        method = NSURLAuthenticationMethodHTTPDigest;
        break;
    case ProtectionSpace::AuthenticationScheme::HTMLForm:
        method = NSURLAuthenticationMethodHTMLForm;
        break;
    case ProtectionSpace::AuthenticationScheme::NTLM:
        method = NSURLAuthenticationMethodNTLM;
        break;
    case ProtectionSpace::AuthenticationScheme::Negotiate:
        method = NSURLAuthenticationMethodNegotiate;
        break;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    case ProtectionSpace::AuthenticationScheme::ServerTrustEvaluationRequested:
        method = NSURLAuthenticationMethodServerTrust;
        break;
    case ProtectionSpace::AuthenticationScheme::ClientCertificateRequested:
        method = NSURLAuthenticationMethodClientCertificate;
        break;
#endif
    case ProtectionSpace::AuthenticationScheme::OAuth:
        method = NSURLAuthenticationMethodOAuth;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (proxyType)
        lazyInitialize(m_nsSpace, adoptNS([[NSURLProtectionSpace alloc] initWithProxyHost:host().createNSString().get() port:port() type:proxyType.get() realm:realm().createNSString().get() authenticationMethod:method]));
    else
        lazyInitialize(m_nsSpace, adoptNS([[NSURLProtectionSpace alloc] initWithHost:host().createNSString().get() port:port() protocol:protocol.get() realm:realm().createNSString().get() authenticationMethod:method]));

    return m_nsSpace.get();
}

bool ProtectionSpace::platformCompare(const ProtectionSpace& a, const ProtectionSpace& b)
{
    if (!a.m_nsSpace && !b.m_nsSpace)
        return true;

    return [protect(a.nsSpace()) isEqual:protect(b.nsSpace()).get()];
}

bool ProtectionSpace::receivesCredentialSecurely() const
{
    return [protect(nsSpace()) receivesCredentialSecurely];
}

bool ProtectionSpace::encodingRequiresPlatformData(NSURLProtectionSpace *space)
{
    return space.distinguishedNames || space.serverTrust;
}

std::optional<ProtectionSpace::PlatformData> ProtectionSpace::getPlatformDataToSerialize() const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isInWebProcess());
    if (encodingRequiresPlatformData())
        return PlatformData { nsSpace() };
    return std::nullopt;
}

bool ProtectionSpace::encodingRequiresPlatformData() const
{
    return m_nsSpace && encodingRequiresPlatformData(m_nsSpace.get());
}

} // namespace WebCore
