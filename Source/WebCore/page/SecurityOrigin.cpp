/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecurityOrigin.h"

#include "BlobURL.h"
#include "LegacySchemeRegistry.h"
#include "OriginAccessEntry.h"
#include "PublicSuffix.h"
#include "RuntimeApplicationChecks.h"
#include "SecurityPolicy.h"
#include <pal/text/TextEncoding.h>
#include "ThreadableBlobRegistry.h"
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

constexpr unsigned maximumURLSize = 0x04000000;

static bool schemeRequiresHost(const URL& url)
{
    // We expect URLs with these schemes to have authority components. If the
    // URL lacks an authority component, we get concerned and mark the origin
    // as opaque.
    return url.protocolIsInHTTPFamily() || url.protocolIs("ftp"_s);
}

bool SecurityOrigin::shouldIgnoreHost(const URL& url)
{
    return url.protocolIsData() || url.protocolIsAbout() || url.protocolIsJavaScript() || url.protocolIs("file"_s);
}

bool SecurityOrigin::shouldUseInnerURL(const URL& url)
{
    // FIXME: Blob URLs don't have inner URLs. Their form is "blob:<inner-origin>/<UUID>", so treating the part after "blob:" as a URL is incorrect.
    if (url.protocolIsBlob())
        return true;
    UNUSED_PARAM(url);
    return false;
}

// In general, extracting the inner URL varies by scheme. It just so happens
// that all the URL schemes we currently support that use inner URLs for their
// security origin can be parsed using this algorithm.
URL SecurityOrigin::extractInnerURL(const URL& url)
{
    // FIXME: Update this callsite to use the innerURL member function when
    // we finish implementing it.
    return URL { PAL::decodeURLEscapeSequences(url.path()) };
}

static RefPtr<SecurityOrigin> getCachedOrigin(const URL& url)
{
    if (url.protocolIsBlob())
        return ThreadableBlobRegistry::getCachedOrigin(url);
    return nullptr;
}

static bool shouldTreatAsOpaqueOrigin(const URL& url)
{
    if (!url.isValid())
        return true;

    // FIXME: Do we need to unwrap the URL further?
    URL innerURL = SecurityOrigin::shouldUseInnerURL(url) ? SecurityOrigin::extractInnerURL(url) : url;
    if (!innerURL.isValid())
        return true;

    // For edge case URLs that were probably misparsed, make sure that the origin is opaque.
    // This is an additional safety net against bugs in URL parsing, and for network back-ends that parse URLs differently,
    // and could misinterpret another component for hostname.
    if (schemeRequiresHost(innerURL) && innerURL.host().isEmpty())
        return true;

    if (LegacySchemeRegistry::shouldTreatURLSchemeAsNoAccess(innerURL.protocol()))
        return true;

    // https://url.spec.whatwg.org/#origin with some additions
    if (url.hasSpecialScheme()
#if PLATFORM(COCOA)
        || !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NullOriginForNonSpecialSchemedURLs)
        || url.protocolIs("applewebdata"_s)
        || url.protocolIs("x-apple-ql-id"_s)
        || url.protocolIs("x-apple-ql-id2"_s)
        || url.protocolIs("x-apple-ql-magic"_s)
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
        || url.protocolIs("resource"_s)
#endif
#if ENABLE(PDFJS)
        || url.protocolIs("webkit-pdfjs-viewer"_s)
#endif
        || url.protocolIs("blob"_s))
        return false;

    return !LegacySchemeRegistry::schemeIsHandledBySchemeHandler(url.protocol());
}

static bool isLoopbackIPAddress(StringView host)
{
    // The IPv6 loopback address is 0:0:0:0:0:0:0:1, which compresses to ::1.
    if (host == "[::1]"_s)
        return true;

    // Check to see if it's a valid IPv4 address that has the form 127.*.*.*.
    if (!host.startsWith("127."_s))
        return false;
    size_t dotsFound = 0;
    for (size_t i = 0; i < host.length(); ++i) {
        if (host[i] == '.') {
            dotsFound++;
            continue;
        }
        if (!isASCIIDigit(host[i]))
            return false;
    }
    return dotsFound == 3;
}

// https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy (Editor's Draft, 17 November 2016)
static bool shouldTreatAsPotentiallyTrustworthy(StringView protocol, StringView host)
{
    if (LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(protocol))
        return true;

    if (SecurityOrigin::isLocalHostOrLoopbackIPAddress(host))
        return true;

    if (LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(protocol))
        return true;

    if (LegacySchemeRegistry::schemeIsHandledBySchemeHandler(protocol))
        return true;

    return false;
}

bool shouldTreatAsPotentiallyTrustworthy(const URL& url)
{
    return shouldTreatAsPotentiallyTrustworthy(url.protocol(), url.host());
}

SecurityOrigin::SecurityOrigin(const URL& url)
    : m_data(SecurityOriginData::fromURL(url))
    , m_isLocal(LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(m_data.protocol))
{
    // document.domain starts as m_data.host, but can be set by the DOM.
    m_domain = m_data.host;

    if (m_data.port && WTF::isDefaultPortForProtocol(m_data.port.value(), m_data.protocol))
        m_data.port = std::nullopt;

    // By default, only local SecurityOrigins can load local resources.
    m_canLoadLocalResources = isLocal();

    if (m_canLoadLocalResources)
        m_filePath = url.fileSystemPath(); // In case enforceFilePathSeparation() is called.
}

SecurityOrigin::SecurityOrigin()
    : m_data { emptyString(), emptyString(), std::nullopt }
    , m_domain { emptyString() }
    , m_opaqueOriginIdentifier { OpaqueOriginIdentifier::generateThreadSafe() }
    , m_isPotentiallyTrustworthy { false }
{
}

SecurityOrigin::SecurityOrigin(const SecurityOrigin* other)
    : m_data { other->m_data.isolatedCopy() }
    , m_domain { other->m_domain.isolatedCopy() }
    , m_filePath { other->m_filePath.isolatedCopy() }
    , m_opaqueOriginIdentifier { other->m_opaqueOriginIdentifier }
    , m_universalAccess { other->m_universalAccess }
    , m_domainWasSetInDOM { other->m_domainWasSetInDOM }
    , m_canLoadLocalResources { other->m_canLoadLocalResources }
    , m_enforcesFilePathSeparation { other->m_enforcesFilePathSeparation }
    , m_needsStorageAccessFromFileURLsQuirk { other->m_needsStorageAccessFromFileURLsQuirk }
    , m_isPotentiallyTrustworthy { other->m_isPotentiallyTrustworthy }
    , m_isLocal { other->m_isLocal }
{
}

Ref<SecurityOrigin> SecurityOrigin::create(const URL& url)
{
    if (RefPtr<SecurityOrigin> cachedOrigin = getCachedOrigin(url))
        return cachedOrigin.releaseNonNull();

    if (shouldTreatAsOpaqueOrigin(url))
        return adoptRef(*new SecurityOrigin);

    if (shouldUseInnerURL(url))
        return adoptRef(*new SecurityOrigin(extractInnerURL(url)));

    return adoptRef(*new SecurityOrigin(url));
}

Ref<SecurityOrigin> SecurityOrigin::createOpaque()
{
    Ref<SecurityOrigin> origin(adoptRef(*new SecurityOrigin));
    ASSERT(origin.get().isOpaque());
    return origin;
}

Ref<SecurityOrigin> SecurityOrigin::createNonLocalWithAllowedFilePath(const URL& url, const String& filePath)
{
    ASSERT(!url.isLocalFile());
    auto securityOrigin = SecurityOrigin::create(url);
    securityOrigin->m_filePath = filePath;
    return securityOrigin;
}

Ref<SecurityOrigin> SecurityOrigin::isolatedCopy() const
{
    return adoptRef(*new SecurityOrigin(this));
}

void SecurityOrigin::setDomainFromDOM(const String& newDomain)
{
    m_domainWasSetInDOM = true;
    m_domain = newDomain.convertToASCIILowercase();
}

bool SecurityOrigin::isSecure(const URL& url)
{
    // Invalid URLs are secure, as are URLs which have a secure protocol.
    if (!url.isValid() || LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(url.protocol()))
        return true;

    // URLs that wrap inner URLs are secure if those inner URLs are secure.
    if (shouldUseInnerURL(url))
        return LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(extractInnerURL(url).protocol()) || BlobURL::isSecureBlobURL(url);

    return false;
}

bool SecurityOrigin::isSameOriginDomain(const SecurityOrigin& other) const
{
    if (m_universalAccess)
        return true;

    if (this == &other)
        return true;

    if (isOpaque() || other.isOpaque())
        return m_opaqueOriginIdentifier == other.m_opaqueOriginIdentifier;

    // Here are two cases where we should permit access:
    //
    // 1) Neither document has set document.domain. In this case, we insist
    //    that the scheme, host, and port of the URLs match.
    //
    // 2) Both documents have set document.domain. In this case, we insist
    //    that the documents have set document.domain to the same value and
    //    that the scheme of the URLs match.
    //
    // This matches the behavior of Firefox 2 and Internet Explorer 6.
    //
    // Internet Explorer 7 and Opera 9 are more strict in that they require
    // the port numbers to match when both pages have document.domain set.
    //
    // FIXME: Evaluate whether we can tighten this policy to require matched
    //        port numbers.
    //
    // Opera 9 allows access when only one page has set document.domain, but
    // this is a security vulnerability.

    bool canAccess = false;
    if (m_data.protocol == other.m_data.protocol) {
        if (!m_domainWasSetInDOM && !other.m_domainWasSetInDOM) {
            if (m_data.host == other.m_data.host && m_data.port == other.m_data.port)
                canAccess = true;
        } else if (m_domainWasSetInDOM && other.m_domainWasSetInDOM) {
            if (m_domain == other.m_domain)
                canAccess = true;
        }
    }

    if (canAccess && isLocal())
        canAccess = passesFileCheck(other);

    return canAccess;
}

bool SecurityOrigin::passesFileCheck(const SecurityOrigin& other) const
{
    ASSERT(isLocal() && other.isLocal());

    return !m_enforcesFilePathSeparation && !other.m_enforcesFilePathSeparation;
}

bool SecurityOrigin::canRequest(const URL& url) const
{
    if (m_universalAccess)
        return true;

    if (getCachedOrigin(url) == this)
        return true;

    if (isOpaque())
        return false;

    Ref<SecurityOrigin> targetOrigin(SecurityOrigin::create(url));

    if (targetOrigin->isOpaque())
        return false;

    // We call isSameSchemeHostPort here instead of canAccess because we want
    // to ignore document.domain effects.
    if (isSameSchemeHostPort(targetOrigin.get()))
        return true;

    if (SecurityPolicy::isAccessAllowed(*this, targetOrigin.get(), url))
        return true;

    return false;
}

bool SecurityOrigin::canReceiveDragData(const SecurityOrigin& dragInitiator) const
{
    if (this == &dragInitiator)
        return true;

    if (dragInitiator.isLocal() && isLocal())
        return true;

    return isSameOriginDomain(dragInitiator);
}

// This is a hack to allow keep navigation to http/https feeds working. To remove this
// we need to introduce new API akin to registerURLSchemeAsLocal, that registers a
// protocols navigation policy.
// feed(|s|search): is considered a 'nesting' scheme by embedders that support it, so it can be
// local or remote depending on what is nested. Currently we just check if we are nesting
// http or https, otherwise we ignore the nesting for the purpose of a security check. We need
// a facility for registering nesting schemes, and some generalized logic for them.
// This function should be removed as an outcome of https://bugs.webkit.org/show_bug.cgi?id=69196
static bool isFeedWithNestedProtocolInHTTPFamily(const URL& url)
{
    const String& string = url.string();
    if (!startsWithLettersIgnoringASCIICase(string, "feed"_s))
        return false;
    return startsWithLettersIgnoringASCIICase(string, "feed://"_s)
        || startsWithLettersIgnoringASCIICase(string, "feed:http:"_s)
        || startsWithLettersIgnoringASCIICase(string, "feed:https:"_s)
        || startsWithLettersIgnoringASCIICase(string, "feeds:http:"_s)
        || startsWithLettersIgnoringASCIICase(string, "feeds:https:"_s)
        || startsWithLettersIgnoringASCIICase(string, "feedsearch:http:"_s)
        || startsWithLettersIgnoringASCIICase(string, "feedsearch:https:"_s);
}

bool SecurityOrigin::canDisplay(const URL& url) const
{
    ASSERT(!isInNetworkProcess());
    if (m_universalAccess)
        return true;

    if (url.pathEnd() > maximumURLSize)
        return false;
    
#if !PLATFORM(IOS_FAMILY) && !ENABLE(BUBBLEWRAP_SANDBOX)
    if (m_data.protocol == "file"_s && url.isLocalFile() && !FileSystem::filesHaveSameVolume(m_filePath, url.fileSystemPath()))
        return false;
#endif

    if (isFeedWithNestedProtocolInHTTPFamily(url))
        return true;

    auto protocol = url.protocol();

    if (LegacySchemeRegistry::canDisplayOnlyIfCanRequest(protocol))
        return canRequest(url);

    if (LegacySchemeRegistry::shouldTreatURLSchemeAsDisplayIsolated(protocol))
        return equalIgnoringASCIICase(m_data.protocol, protocol) || SecurityPolicy::isAccessAllowed(*this, url);

    if (!SecurityPolicy::restrictAccessToLocal())
        return true;

    if (url.isLocalFile() && url.fileSystemPath() == m_filePath)
        return true;

    if (LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(protocol))
        return canLoadLocalResources() || SecurityPolicy::isAccessAllowed(*this, url);

    return true;
}

SecurityOrigin::Policy SecurityOrigin::canShowNotifications() const
{
    if (m_universalAccess)
        return AlwaysAllow;
    if (isOpaque())
        return AlwaysDeny;
    return Ask;
}

bool SecurityOrigin::isSameOriginAs(const SecurityOrigin& other) const
{
    if (this == &other)
        return true;

    if (isOpaque() || other.isOpaque())
        return m_opaqueOriginIdentifier == other.m_opaqueOriginIdentifier;

    return isSameSchemeHostPort(other);
}

bool SecurityOrigin::isSameSiteAs(const SecurityOrigin& other) const
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    // https://html.spec.whatwg.org/#same-site
    if (isOpaque() != other.isOpaque())
        return false;
    if (!isOpaque() && protocol() != other.protocol())
        return false;

    if (isOpaque())
        return isSameOriginAs(other);

    auto topDomain = topPrivatelyControlledDomain(domain());
    if (topDomain.isEmpty())
        return host() == other.host();

    return topDomain == topPrivatelyControlledDomain(other.domain());
#else
    return isSameOriginAs(other);
#endif // ENABLE(PUBLIC_SUFFIX_LIST)
}

bool SecurityOrigin::isMatchingRegistrableDomainSuffix(const String& domainSuffix, bool treatIPAddressAsDomain) const
{
    if (domainSuffix.isEmpty())
        return false;

    auto ipAddressSetting = treatIPAddressAsDomain ? OriginAccessEntry::TreatIPAddressAsDomain : OriginAccessEntry::TreatIPAddressAsIPAddress;
    OriginAccessEntry accessEntry { protocol(), domainSuffix, OriginAccessEntry::AllowSubdomains, ipAddressSetting };
    if (!accessEntry.matchesOrigin(*this))
        return false;

    // Always return true if it is an exact match.
    if (domainSuffix.length() == host().length())
        return true;

#if ENABLE(PUBLIC_SUFFIX_LIST)
    return !isPublicSuffix(domainSuffix);
#else
    return true;
#endif
}

bool SecurityOrigin::isPotentiallyTrustworthy() const
{
    if (!m_isPotentiallyTrustworthy)
        m_isPotentiallyTrustworthy = shouldTreatAsPotentiallyTrustworthy(m_data.protocol, m_data.host);
    return *m_isPotentiallyTrustworthy;
}

void SecurityOrigin::grantLoadLocalResources()
{
    // Granting privileges to some, but not all, documents in a SecurityOrigin
    // is a security hazard because the documents without the privilege can
    // obtain the privilege by injecting script into the documents that have
    // been granted the privilege.
    m_canLoadLocalResources = true;
}

void SecurityOrigin::grantUniversalAccess()
{
    m_universalAccess = true;
}

void SecurityOrigin::grantStorageAccessFromFileURLsQuirk()
{
    m_needsStorageAccessFromFileURLsQuirk = true;
}

String SecurityOrigin::domainForCachePartition() const
{
    if (isHTTPFamily())
        return host();

    if (LegacySchemeRegistry::shouldPartitionCacheForURLScheme(m_data.protocol))
        return host();

    return emptyString();
}

void SecurityOrigin::setEnforcesFilePathSeparation()
{
    ASSERT(isLocal());
    m_enforcesFilePathSeparation = true;
}

String SecurityOrigin::toString() const
{
    if (isOpaque())
        return "null"_s;
    if (m_data.protocol == "file"_s && m_enforcesFilePathSeparation)
        return "null"_s;
    return toRawString();
}

String SecurityOrigin::toRawString() const
{
    return m_data.toString();
}

URL SecurityOrigin::toURL() const
{
    return m_data.toURL();
}

static inline bool areOriginsMatching(const SecurityOrigin& origin1, const SecurityOrigin& origin2)
{
    ASSERT(&origin1 != &origin2);

    if (origin1.isOpaque() || origin2.isOpaque())
        return origin1.isOpaque() == origin2.isOpaque();

    if (origin1.protocol() != origin2.protocol())
        return false;

    if (origin1.protocol() == "file"_s)
        return origin1.enforcesFilePathSeparation() == origin2.enforcesFilePathSeparation();

    if (origin1.host() != origin2.host())
        return false;

    return origin1.port() == origin2.port();
}

// This function mimics the result of string comparison of serialized origins.
bool serializedOriginsMatch(const SecurityOrigin& origin1, const SecurityOrigin& origin2)
{
    if (&origin1 == &origin2)
        return true;

    ASSERT(!areOriginsMatching(origin1, origin2) || (origin1.toString() == origin2.toString()));
    return areOriginsMatching(origin1, origin2);
}

bool serializedOriginsMatch(const SecurityOrigin* origin1, const SecurityOrigin* origin2)
{
    if (!origin1 || !origin2)
        return origin1 == origin2;

    return serializedOriginsMatch(*origin1, *origin2);
}

Ref<SecurityOrigin> SecurityOrigin::createFromString(const String& originString)
{
    return SecurityOrigin::create(URL { originString });
}

Ref<SecurityOrigin> SecurityOrigin::create(const String& protocol, const String& host, std::optional<uint16_t> port)
{
    String decodedHost = PAL::decodeURLEscapeSequences(host);
    auto origin = create(URL { protocol + "://" + host + "/" });
    if (port && !WTF::isDefaultPortForProtocol(*port, protocol))
        origin->m_data.port = port;
    return origin;
}

bool SecurityOrigin::equal(const SecurityOrigin* other) const 
{
    if (other == this)
        return true;

    if (isOpaque() || other->isOpaque())
        return m_opaqueOriginIdentifier == other->m_opaqueOriginIdentifier;
    
    if (!isSameSchemeHostPort(*other))
        return false;

    if (m_domainWasSetInDOM != other->m_domainWasSetInDOM)
        return false;

    if (m_domainWasSetInDOM && m_domain != other->m_domain)
        return false;

    return true;
}

bool SecurityOrigin::isSameSchemeHostPort(const SecurityOrigin& other) const
{
    if (m_data != other.m_data)
        return false;

    if (isLocal() && !passesFileCheck(other))
        return false;

    return true;
}

bool SecurityOrigin::isLocalHostOrLoopbackIPAddress(StringView host)
{
    if (isLoopbackIPAddress(host))
        return true;

    // FIXME: Ensure that localhost resolves to the loopback address.
    if (equalLettersIgnoringASCIICase(host, "localhost"_s) || host.endsWithIgnoringASCIICase(".localhost"_s))
        return true;

    return false;
}

} // namespace WebCore
