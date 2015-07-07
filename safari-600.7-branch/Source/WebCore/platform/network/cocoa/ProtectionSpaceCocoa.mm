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

#if USE(CFNETWORK)
@interface NSURLProtectionSpace (WebDetails)
- (CFURLProtectionSpaceRef) _CFURLProtectionSpace;
- (id)_initWithCFURLProtectionSpace:(CFURLProtectionSpaceRef)cfProtSpace;
@end
#endif

namespace WebCore {

#if USE(CFNETWORK)
ProtectionSpace::ProtectionSpace(CFURLProtectionSpaceRef space)
    : ProtectionSpace(adoptNS([[NSURLProtectionSpace alloc] _initWithCFURLProtectionSpace:space]).get())
{
}
#endif

static ProtectionSpaceServerType type(NSURLProtectionSpace *space)
{
    if ([space isProxy]) {
        NSString *proxyType = space.proxyType;
        if ([proxyType isEqualToString:NSURLProtectionSpaceHTTPProxy])
            return ProtectionSpaceProxyHTTP;
        if ([proxyType isEqualToString:NSURLProtectionSpaceHTTPSProxy])
            return ProtectionSpaceProxyHTTPS;
        if ([proxyType isEqualToString:NSURLProtectionSpaceFTPProxy])
            return ProtectionSpaceProxyFTP;
        if ([proxyType isEqualToString:NSURLProtectionSpaceSOCKSProxy])
            return ProtectionSpaceProxySOCKS;

        ASSERT_NOT_REACHED();
        return ProtectionSpaceProxyHTTP;
    }

    NSString *protocol = space.protocol;
    if ([protocol caseInsensitiveCompare:@"http"] == NSOrderedSame)
        return ProtectionSpaceServerHTTP;
    if ([protocol caseInsensitiveCompare:@"https"] == NSOrderedSame)
        return ProtectionSpaceServerHTTPS;
    if ([protocol caseInsensitiveCompare:@"ftp"] == NSOrderedSame)
        return ProtectionSpaceServerFTP;
    if ([protocol caseInsensitiveCompare:@"ftps"] == NSOrderedSame)
        return ProtectionSpaceServerFTPS;

    ASSERT_NOT_REACHED();
    return ProtectionSpaceServerHTTP;
}

static ProtectionSpaceAuthenticationScheme scheme(NSURLProtectionSpace *space)
{
    NSString *method = space.authenticationMethod;
    if ([method isEqualToString:NSURLAuthenticationMethodDefault])
        return ProtectionSpaceAuthenticationSchemeDefault;
    if ([method isEqualToString:NSURLAuthenticationMethodHTTPBasic])
        return ProtectionSpaceAuthenticationSchemeHTTPBasic;
    if ([method isEqualToString:NSURLAuthenticationMethodHTTPDigest])
        return ProtectionSpaceAuthenticationSchemeHTTPDigest;
    if ([method isEqualToString:NSURLAuthenticationMethodHTMLForm])
        return ProtectionSpaceAuthenticationSchemeHTMLForm;
    if ([method isEqualToString:NSURLAuthenticationMethodNTLM])
        return ProtectionSpaceAuthenticationSchemeNTLM;
    if ([method isEqualToString:NSURLAuthenticationMethodNegotiate])
        return ProtectionSpaceAuthenticationSchemeNegotiate;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    if ([method isEqualToString:NSURLAuthenticationMethodClientCertificate])
        return ProtectionSpaceAuthenticationSchemeClientCertificateRequested;
    if ([method isEqualToString:NSURLAuthenticationMethodServerTrust])
        return ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;
#endif

    ASSERT_NOT_REACHED();
    return ProtectionSpaceAuthenticationSchemeUnknown;
}

ProtectionSpace::ProtectionSpace(NSURLProtectionSpace *space)
    : ProtectionSpace(space.host, space.port, type(space), space.realm, scheme(space))
{
    m_nsSpace = space;
}

#if USE(CFNETWORK)
CFURLProtectionSpaceRef ProtectionSpace::cfSpace() const
{
    return [nsSpace() _CFURLProtectionSpace];
}
#endif

NSURLProtectionSpace *ProtectionSpace::nsSpace() const
{
    if (m_nsSpace)
        return m_nsSpace.get();

    NSString *proxyType = nil;
    NSString *protocol = nil;
    switch (serverType()) {
    case ProtectionSpaceServerHTTP:
        protocol = @"http";
        break;
    case ProtectionSpaceServerHTTPS:
        protocol = @"https";
        break;
    case ProtectionSpaceServerFTP:
        protocol = @"ftp";
        break;
    case ProtectionSpaceServerFTPS:
        protocol = @"ftps";
        break;
    case ProtectionSpaceProxyHTTP:
        proxyType = NSURLProtectionSpaceHTTPProxy;
        break;
    case ProtectionSpaceProxyHTTPS:
        proxyType = NSURLProtectionSpaceHTTPSProxy;
        break;
    case ProtectionSpaceProxyFTP:
        proxyType = NSURLProtectionSpaceFTPProxy;
        break;
    case ProtectionSpaceProxySOCKS:
        proxyType = NSURLProtectionSpaceSOCKSProxy;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
  
    NSString *method = nil;
    switch (authenticationScheme()) {
    case ProtectionSpaceAuthenticationSchemeDefault:
        method = NSURLAuthenticationMethodDefault;
        break;
    case ProtectionSpaceAuthenticationSchemeHTTPBasic:
        method = NSURLAuthenticationMethodHTTPBasic;
        break;
    case ProtectionSpaceAuthenticationSchemeHTTPDigest:
        method = NSURLAuthenticationMethodHTTPDigest;
        break;
    case ProtectionSpaceAuthenticationSchemeHTMLForm:
        method = NSURLAuthenticationMethodHTMLForm;
        break;
    case ProtectionSpaceAuthenticationSchemeNTLM:
        method = NSURLAuthenticationMethodNTLM;
        break;
    case ProtectionSpaceAuthenticationSchemeNegotiate:
        method = NSURLAuthenticationMethodNegotiate;
        break;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    case ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        method = NSURLAuthenticationMethodServerTrust;
        break;
    case ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        method = NSURLAuthenticationMethodClientCertificate;
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    
    m_nsSpace = adoptNS(proxyType
        ? [[NSURLProtectionSpace alloc] initWithProxyHost:host() port:port() type:proxyType realm:realm() authenticationMethod:method]
        : [[NSURLProtectionSpace alloc] initWithHost:host() port:port() protocol:protocol realm:realm() authenticationMethod:method]);

    return m_nsSpace.get();
}

bool ProtectionSpace::platformCompare(const ProtectionSpace& a, const ProtectionSpace& b)
{
    if (!a.m_nsSpace && !b.m_nsSpace)
        return true;

    return [a.nsSpace() isEqual:b.nsSpace()];
}

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
bool ProtectionSpace::encodingRequiresPlatformData(NSURLProtectionSpace *space)
{
    return space.distinguishedNames || space.serverTrust;
}
#endif

} // namespace WebCore
