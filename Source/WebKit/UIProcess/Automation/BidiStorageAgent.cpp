/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BidiStorageAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "BidiBrowserAgent.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProcessor.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/Cookie.h>
#include <wtf/Borrow.h>
#include <wtf/Deque.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Base64.h>

namespace WebKit {

using namespace Inspector;
using BytesValue = Inspector::Protocol::BidiStorage::BytesValue;
using BytesValueType = Inspector::Protocol::BidiStorage::BytesValueType;
using CookieSameSitePolicy = Inspector::Protocol::BidiStorage::CookieSameSitePolicy;
using PartitionKey = Inspector::Protocol::BidiStorage::PartitionKey;
using ProtocolCookie = Inspector::Protocol::BidiStorage::Cookie;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiStorageAgent);

BidiStorageAgent::BidiStorageAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_storageDomainDispatcher(BidiStorageBackendDispatcher::create(backendDispatcher, this))
{
}

BidiStorageAgent::~BidiStorageAgent() = default;

// MARK: Cookies.

// WebKit stores the Domain attribute of a domain cookie with a leading dot. The stored cookie's
// domain in https://httpwg.org/specs/rfc6265.html#storage-model has none.
static bool isDomainCookie(const WebCore::Cookie& cookie)
{
    return cookie.domain.startsWith('.');
}

static String domainForCookie(const WebCore::Cookie& cookie)
{
    if (isDomainCookie(cookie))
        return cookie.domain.substring(1);
    return cookie.domain;
}

static Vector<uint8_t> utf8Bytes(const String& string)
{
    auto utf8 = string.utf8();
    return Vector<uint8_t> { byteCast<uint8_t>(utf8.span()) };
}

// The byte length of the cookie as it would appear in a Cookie header. Like other implementations
// and the WPT tests, this counts the name and the value but not the "=" between them.
static uint64_t sizeForCookie(const WebCore::Cookie& cookie)
{
    return cookie.name.utf8().length() + cookie.value.utf8().length();
}

static CookieSameSitePolicy sameSiteForCookie(const WebCore::Cookie& cookie)
{
    switch (cookie.sameSite) {
    case WebCore::Cookie::SameSitePolicy::None:
        return CookieSameSitePolicy::None;
    case WebCore::Cookie::SameSitePolicy::Lax:
        return CookieSameSitePolicy::Lax;
    case WebCore::Cookie::SameSitePolicy::Strict:
        return CookieSameSitePolicy::Strict;
    }
    ASSERT_NOT_REACHED();
    return CookieSameSitePolicy::None;
}

// WebCore::Cookie::expires is in milliseconds since the epoch; network.Cookie's expiry is in seconds.
static std::optional<uint64_t> expiryForCookie(const WebCore::Cookie& cookie)
{
    if (cookie.session || !cookie.expires || *cookie.expires < 0)
        return std::nullopt;
    return static_cast<uint64_t>(*cookie.expires / 1000);
}

Ref<ProtocolCookie> BidiStorageAgent::serializeCookie(const WebCore::Cookie& cookie)
{
    // FIXME: serialize a value that is not valid UTF-8 as base64. WebCore::Cookie keeps the value as a String,
    // so the original bytes are not available here.
    auto value = BytesValue::create()
        .setType(BytesValueType::String)
        .setValue(cookie.value)
        .release();

    auto result = ProtocolCookie::create()
        .setName(cookie.name)
        .setValue(WTF::move(value))
        .setDomain(domainForCookie(cookie))
        .setPath(cookie.path)
        .setSize(static_cast<int>(std::min<uint64_t>(sizeForCookie(cookie), std::numeric_limits<int>::max())))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSameSite(sameSiteForCookie(cookie))
        .release();

    if (auto expiry = expiryForCookie(cookie))
        result->setExpiry(static_cast<double>(*expiry));

    return result;
}

// MARK: Cookie filters.

static std::optional<String> stringMember(const JSON::Object& object, const String& name, bool& isInvalid)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::nullopt;
    auto string = value->asString();
    if (string.isNull())
        isInvalid = true;
    return string;
}

static std::optional<bool> booleanMember(const JSON::Object& object, const String& name, bool& isInvalid)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::nullopt;
    auto boolean = value->asBoolean();
    if (!boolean)
        isInvalid = true;
    return boolean;
}

// A js-uint: an integer from 0 to 2^53 - 1.
static constexpr double maxSafeInteger = 9007199254740991.0;

static std::optional<uint64_t> unsignedIntegerMember(const JSON::Object& object, const String& name, bool& isInvalid)
{
    RefPtr value = object.getValue(name);
    if (!value)
        return std::nullopt;
    auto number = value->asDouble();
    if (!number || *number < 0 || *number > maxSafeInteger || std::trunc(*number) != *number) {
        isInvalid = true;
        return std::nullopt;
    }
    return static_cast<uint64_t>(*number);
}

// https://w3c.github.io/webdriver-bidi/#deserialize-protocol-bytes
static Inspector::Protocol::ErrorStringOr<Vector<uint8_t>> deserializeProtocolBytes(const JSON::Value& protocolBytes)
{
    RefPtr object = protocolBytes.asObject();
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!object, InvalidParameter);

    auto type = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<BytesValueType>(object->getString("type"_s));
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!type, InvalidParameter);

    auto value = object->getString("value"_s);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(value.isNull(), InvalidParameter);

    switch (*type) {
    case BytesValueType::String:
        return utf8Bytes(value);
    case BytesValueType::Base64: {
        auto bytes = base64Decode(value);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!bytes, InvalidParameter, "value is not valid base64"_s);
        return WTF::move(*bytes);
    }
    }

    SYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);
}

Inspector::Protocol::ErrorStringOr<BidiCookieFilter> BidiCookieFilter::fromProtocol(const RefPtr<JSON::Object>& protocolFilter)
{
    BidiCookieFilter filter;
    if (!protocolFilter)
        return filter;

    bool isInvalid = false;
    filter.name = stringMember(*protocolFilter, "name"_s, isInvalid);
    filter.domain = stringMember(*protocolFilter, "domain"_s, isInvalid);
    filter.path = stringMember(*protocolFilter, "path"_s, isInvalid);
    filter.size = unsignedIntegerMember(*protocolFilter, "size"_s, isInvalid);
    filter.httpOnly = booleanMember(*protocolFilter, "httpOnly"_s, isInvalid);
    filter.secure = booleanMember(*protocolFilter, "secure"_s, isInvalid);
    filter.expiry = unsignedIntegerMember(*protocolFilter, "expiry"_s, isInvalid);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(isInvalid, InvalidParameter);

    if (auto sameSite = stringMember(*protocolFilter, "sameSite"_s, isInvalid)) {
        SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(isInvalid, InvalidParameter);
        filter.sameSite = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<CookieSameSitePolicy>(*sameSite);
        SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!filter.sameSite, InvalidParameter);
    }

    if (RefPtr value = protocolFilter->getValue("value"_s)) {
        auto bytes = deserializeProtocolBytes(*value);
        if (!bytes)
            return makeUnexpected(bytes.error());
        filter.value = WTF::move(*bytes);
    }

    return filter;
}

bool BidiCookieFilter::matches(const WebCore::Cookie& cookie) const
{
    if (name && *name != cookie.name)
        return false;
    if (value && *value != utf8Bytes(cookie.value))
        return false;
    if (domain && *domain != domainForCookie(cookie))
        return false;
    if (path && *path != cookie.path)
        return false;
    if (size && *size != sizeForCookie(cookie))
        return false;
    if (httpOnly && *httpOnly != cookie.httpOnly)
        return false;
    if (secure && *secure != cookie.secure)
        return false;
    if (sameSite && *sameSite != sameSiteForCookie(cookie))
        return false;
    if (expiry && *expiry != expiryForCookie(cookie))
        return false;
    return true;
}

// MARK: Storage partitions.

Ref<API::HTTPCookieStore> BidiStoragePartition::cookieStore() const
{
    return protect(dataStore.get())->cookieStore();
}

// https://httpwg.org/specs/rfc6265.html#domain-matching, for cookies that resources of the source origin can access.
bool BidiStoragePartition::containsCookie(const WebCore::Cookie& cookie) const
{
    if (sourceOriginHost.isNull())
        return true;

    auto cookieDomain = domainForCookie(cookie);
    if (equalIgnoringASCIICase(sourceOriginHost, cookieDomain))
        return true;
    if (!isDomainCookie(cookie))
        return false;
    return sourceOriginHost.length() > cookieDomain.length()
        && sourceOriginHost.endsWithIgnoringASCIICase(cookieDomain)
        && sourceOriginHost[sourceOriginHost.length() - cookieDomain.length() - 1] == '.';
}

Inspector::Protocol::ErrorStringOr<BidiStoragePartition> BidiStorageAgent::resolveStoragePartition(const RefPtr<JSON::Object>& partitionSpec)
{
    RefPtr session = m_session;
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    auto& browserAgent = session->bidiProcessor().browserAgent();
    auto partitionKey = PartitionKey::create().release();

    if (partitionSpec) {
        auto type = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<Inspector::Protocol::BidiStorage::PartitionDescriptorType>(partitionSpec->getString("type"_s));
        SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!type, InvalidParameter);

        if (*type == Inspector::Protocol::BidiStorage::PartitionDescriptorType::Context) {
            RefPtr contextValue = partitionSpec->getValue("context"_s);
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!contextValue, InvalidParameter);
            auto context = contextValue->asString();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(context.isNull(), InvalidParameter);

            // https://w3c.github.io/webdriver-bidi/#get-a-navigable. The context may be a child frame in another
            // web process with site isolation; the frame's page, and so its data store, is known in the UI process.
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(context.isEmpty(), FrameNotFound);
            auto handles = session->extractBrowsingContextHandles(context);
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!handles, FrameNotFound);
            auto [pageHandle, frameHandle] = handles.value();
            RefPtr page = session->webPageProxyForHandle(pageHandle);
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, FrameNotFound);
            if (!frameHandle.isEmpty()) {
                bool frameNotFound = false;
                auto frameID = session->webFrameIDForHandle(frameHandle, frameNotFound);
                RefPtr frame = frameID && !frameNotFound ? WebFrameProxy::webFrame(*frameID) : nullptr;
                SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!frame || frame->page() != page.get(), FrameNotFound);
            }

            // The navigable's storage partition is its user context's cookie store, which WebKit does not partition by origin.
            partitionKey->setUserContext(browserAgent.userContextForPage(*page));
            return BidiStoragePartition { protect(page->websiteDataStore()), WTF::move(partitionKey), { } };
        }
    }

    // The default values for storage partition key attributes are { userContext: "default" }. There are no
    // required partition key attributes. sourceOrigin has no default: when given, it is kept in the key and
    // limits the partition to that origin's cookies.
    String userContext = "default"_s;
    String sourceOrigin;
    if (partitionSpec) {
        if (RefPtr userContextValue = partitionSpec->getValue("userContext"_s)) {
            userContext = userContextValue->asString();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(userContext.isNull(), InvalidParameter);
        }
        if (RefPtr sourceOriginValue = partitionSpec->getValue("sourceOrigin"_s)) {
            sourceOrigin = sourceOriginValue->asString();
            SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(sourceOrigin.isNull(), InvalidParameter);
        }
    }

    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session->isValidUserContext(userContext), NoSuchUserContext);
    RefPtr dataStore = browserAgent.dataStoreForUserContext(userContext);
    SYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!dataStore, NoSuchStoragePartition);

    String sourceOriginHost;
    if (!sourceOrigin.isNull()) {
        URL sourceOriginURL { sourceOrigin };
        SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sourceOriginURL.isValid() || sourceOriginURL.host().isEmpty(), InvalidParameter, "sourceOrigin is not a valid origin"_s);
        sourceOriginHost = sourceOriginURL.host().toString();
        partitionKey->setSourceOrigin(sourceOrigin);
    }

    partitionKey->setUserContext(userContext);
    return BidiStoragePartition { dataStore.releaseNonNull(), WTF::move(partitionKey), WTF::move(sourceOriginHost) };
}

// MARK: Inspector::BidiStorageDispatcherHandler methods.

// https://w3c.github.io/webdriver-bidi/#command-storage-getCookies
void BidiStorageAgent::getCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallbackOf<Ref<JSON::ArrayOf<ProtocolCookie>>, Ref<PartitionKey>>&& callback)
{
    auto filter = BidiCookieFilter::fromProtocol(optionalFilter);
    if (!filter) {
        callback(makeUnexpected(filter.error()));
        return;
    }

    auto partition = resolveStoragePartition(optionalPartition);
    if (!partition) {
        callback(makeUnexpected(partition.error()));
        return;
    }

    Ref cookieStore = partition->cookieStore();
    cookieStore->cookies([callback = WTF::move(callback), filter = WTF::move(*filter), partition = WTF::move(*partition)](Vector<WebCore::Cookie>&& storedCookies) mutable {
        auto cookies = JSON::ArrayOf<ProtocolCookie>::create();
        for (const auto& cookie : borrow(storedCookies)) {
            if (partition.containsCookie(cookie) && filter.matches(cookie))
                cookies->addItem(serializeCookie(cookie));
        }
        callback({ { WTF::move(cookies), WTF::move(partition.partitionKey) } });
    });
}

// FIXME: setCookie is being reworked in https://bugs.webkit.org/show_bug.cgi?id=288111.
void BidiStorageAgent::setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<PartitionKey>>&& callback)
{
    auto partition = resolveStoragePartition(optionalPartition);
    if (!partition) {
        callback(makeUnexpected(partition.error()));
        return;
    }

    String name = cookie->getString("name"_s);
    String value = cookie->getString("value"_s);
    String domain = cookie->getString("domain"_s);

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(name.isEmpty(), InternalError);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(value.isEmpty(), InternalError);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(domain.isEmpty(), InternalError);

    WebCore::Cookie webCoreCookie;
    webCoreCookie.name = name;
    webCoreCookie.value = value;
    webCoreCookie.domain = domain;
    webCoreCookie.path = cookie->getString("path"_s);

    auto secureOptional = cookie->getBoolean("secure"_s);
    webCoreCookie.secure = secureOptional.value_or(false);

    auto httpOnlyOptional = cookie->getBoolean("httpOnly"_s);
    webCoreCookie.httpOnly = httpOnlyOptional.value_or(false);

    Vector<WebCore::Cookie> cookiesToSet;
    cookiesToSet.append(WTF::move(webCoreCookie));

    Ref cookieStore = partition->cookieStore();
    cookieStore->setCookies(WTF::move(cookiesToSet), [callback = WTF::move(callback), partitionKey = WTF::move(partition->partitionKey)] mutable {
        callback({ WTF::move(partitionKey) });
    });
};

static void deleteCookiesSequentially(RefPtr<API::HTTPCookieStore> store, Deque<WebCore::Cookie> cookies, Ref<PartitionKey> partitionKey, Inspector::CommandCallback<Ref<PartitionKey>> callback)
{
    if (cookies.isEmpty()) {
        callback({ partitionKey });
        return;
    }
    WebCore::Cookie cookie = cookies.takeFirst();
    store->deleteCookie(cookie, [store, cookies = WTF::move(cookies), partitionKey = WTF::move(partitionKey), callback = WTF::move(callback)] mutable {
        deleteCookiesSequentially(store, WTF::move(cookies), partitionKey, WTF::move(callback));
    });
}

// FIXME: deleteCookies is being reworked in https://bugs.webkit.org/show_bug.cgi?id=288112.
void BidiStorageAgent::deleteCookies(RefPtr<JSON::Object>&& optionalFilter, RefPtr<JSON::Object>&& optionalPartition, Inspector::CommandCallback<Ref<PartitionKey>>&& callback)
{
    auto filter = BidiCookieFilter::fromProtocol(optionalFilter);
    if (!filter) {
        callback(makeUnexpected(filter.error()));
        return;
    }

    auto partition = resolveStoragePartition(optionalPartition);
    if (!partition) {
        callback(makeUnexpected(partition.error()));
        return;
    }

    Ref cookieStore = partition->cookieStore();
    cookieStore->cookies([callback = WTF::move(callback), filter = WTF::move(*filter), partition = WTF::move(*partition), cookieStore](Vector<WebCore::Cookie>&& fetchedCookies) mutable {
        Deque<WebCore::Cookie> toDelete;

        for (auto& cookie : borrow(fetchedCookies)) {
            if (partition.containsCookie(cookie) && filter.matches(cookie))
                toDelete.append(cookie);
        }

        if (toDelete.isEmpty()) {
            callback({ WTF::move(partition.partitionKey) });
            return;
        }

        LOG(Automation, "deleteCookies: %zu cookies matched; deleting one-by-one.", toDelete.size());
        deleteCookiesSequentially(WTF::move(cookieStore), WTF::move(toDelete), WTF::move(partition.partitionKey), WTF::move(callback));
    });
}

}

#endif // ENABLE(WEBDRIVER_BIDI)
