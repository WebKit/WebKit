/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "CookieStore.h"

#include "ContextDestructionObserverInlines.h"
#include "Cookie.h"
#include "CookieChangeEvent.h"
#include "CookieChangeEventInit.h"
#include "CookieInit.h"
#include "CookieJar.h"
#include "CookieListItem.h"
#include "CookieStoreDeleteOptions.h"
#include "CookieStoreGetOptions.h"
#include "DocumentPage.h"
#include "EventNames.h"
#include "EventTargetInterfaces.h"
#include "ExceptionOr.h"
#include "JSCookieListItem.h"
#include "JSDOMConvertDictionary.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMPromiseDeferred.h"
#include "PublicSuffixStore.h"
#include "ScriptExecutionContext.h"
#include "ScriptExecutionContextIdentifier.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "TaskSource.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <cmath>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CookieStore);

class CookieStore::MainThreadBridge : public ThreadSafeRefCounted<MainThreadBridge, WTF::DestructionThread::Main> {
public:
    NODELETE static Ref<MainThreadBridge> create(CookieStore& cookieStore)
    {
        return adoptRef(*new MainThreadBridge(cookieStore));
    }

    void get(CookieStoreGetOptions&&, URL&&, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&&);
    void set(Cookie&&, URL&&, Function<void(CookieStore&, std::optional<Exception>&&)>&&);

    NODELETE void detach() { m_cookieStore = nullptr; }

private:
    explicit MainThreadBridge(CookieStore&);

    void ensureOnMainThread(Function<void(ScriptExecutionContext&)>&&);
    void ensureOnContextThread(Function<void(CookieStore&)>&&);

    WeakPtr<CookieStore, WeakPtrImplWithEventTargetData> m_cookieStore;
    Markable<ScriptExecutionContextIdentifier> m_contextIdentifier;
};

CookieStore::MainThreadBridge::MainThreadBridge(CookieStore& cookieStore)
    : m_cookieStore(cookieStore)
    , m_contextIdentifier(cookieStore.scriptExecutionContext() ? std::optional { cookieStore.scriptExecutionContext()->identifier() } : std::nullopt)
{
}

void CookieStore::MainThreadBridge::ensureOnMainThread(Function<void(ScriptExecutionContext&)>&& task)
{
    ASSERT(m_cookieStore);

    RefPtr context = m_cookieStore->scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());

    if (is<Document>(*context)) {
        task(*context);
        return;
    }

    protect(downcast<WorkerGlobalScope>(*context).thread()->workerLoaderProxy())->postTaskToLoader(WTF::move(task));
}

void CookieStore::MainThreadBridge::ensureOnContextThread(Function<void(CookieStore&)>&& task)
{
    ScriptExecutionContext::ensureOnContextThread(*m_contextIdentifier, [protectedThis = Ref { *this }, task = WTF::move(task)](auto&) {
        if (RefPtr cookieStore = protectedThis->m_cookieStore.get())
            task(*cookieStore);
    });
}

void CookieStore::MainThreadBridge::get(CookieStoreGetOptions&& options, URL&& url, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&& completionHandler)
{
    ASSERT(m_cookieStore);

    auto getCookies = [protectedThis = Ref { *this }, options = crossThreadCopy(WTF::move(options)), url = crossThreadCopy(WTF::move(url)), completionHandler = WTF::move(completionHandler)](ScriptExecutionContext& context) mutable {
        Ref document = downcast<Document>(context);
        WeakPtr page = document->page();
        if (!page) {
            protectedThis->ensureOnContextThread([completionHandler = WTF::move(completionHandler)](CookieStore& cookieStore) mutable {
                completionHandler(cookieStore, Exception { ExceptionCode::SecurityError });
            });
            return;
        }

        Ref cookieJar = page->cookieJar();
        auto resultHandler = [protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)] (std::optional<Vector<Cookie>>&& cookies) mutable {
            protectedThis->ensureOnContextThread([completionHandler = WTF::move(completionHandler), cookies = crossThreadCopy(WTF::move(cookies))](CookieStore& cookieStore) mutable {
                if (!cookies)
                    completionHandler(cookieStore, Exception { ExceptionCode::TypeError });
                else
                    completionHandler(cookieStore, WTF::move(*cookies));
            });
        };

        cookieJar->getCookiesAsync(document, url, options, WTF::move(resultHandler));
    };

    ensureOnMainThread(WTF::move(getCookies));
}

void CookieStore::MainThreadBridge::set(Cookie&& cookie, URL&& url, Function<void(CookieStore&, std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(m_cookieStore);

    auto setCookie = [this, protectedThis = Ref { *this }, cookie = crossThreadCopy(WTF::move(cookie)), url = crossThreadCopy(WTF::move(url)), completionHandler = WTF::move(completionHandler)](ScriptExecutionContext& context) mutable {
        Ref document = downcast<Document>(context);
        WeakPtr page = document->page();
        if (!page) {
            ensureOnContextThread([completionHandler = WTF::move(completionHandler)](CookieStore& cookieStore) mutable {
                completionHandler(cookieStore, Exception { ExceptionCode::SecurityError });
            });
            return;
        }

        Ref cookieJar = page->cookieJar();
        auto resultHandler = [this, protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)] (bool setSuccessfully) mutable {
            ensureOnContextThread([completionHandler = WTF::move(completionHandler), setSuccessfully](CookieStore& cookieStore) mutable {
                if (!setSuccessfully)
                    completionHandler(cookieStore, Exception { ExceptionCode::TypeError });
                else
                    completionHandler(cookieStore, std::nullopt);
            });
        };

        document->invalidateDOMCookieCache();
        cookieJar->setCookieAsync(document, url, cookie, WTF::move(resultHandler));
    };

    ensureOnMainThread(WTF::move(setCookie));
}

Ref<CookieStore> CookieStore::create(ScriptExecutionContext* context)
{
    auto cookieStore = adoptRef(*new CookieStore(context));
    cookieStore->suspendIfNeeded();
    return cookieStore;
}

CookieStore::CookieStore(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_mainThreadBridge(MainThreadBridge::create(*this))
{
}

CookieStore::~CookieStore()
{
    m_mainThreadBridge->detach();
}

static RefPtr<StringImpl> normalize(StringImpl* string)
{
    if (contains(string, isTabOrSpace<char16_t>))
        return trim(string, isTabOrSpace);
    return string;
}

static bool containsInvalidCharacters(StringImpl* string)
{
    // The invalid characters are specified at https://cookiestore.spec.whatwg.org/#set-a-cookie.
    return contains(string, [](char16_t character) {
        return character == 0x003B || character == 0x007F || (character <= 0x001F && character != 0x0009);
    });
}

void CookieStore::get(RefPtr<StringImpl>&& name, Ref<DeferredPromise>&& promise)
{
    getShared(GetType::Get, CookieStoreGetOptions { WTF::move(name), { } }, WTF::move(promise));
}

void CookieStore::get(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
{
    getShared(GetType::Get, WTF::move(options), WTF::move(promise));
}

void CookieStore::getAll(RefPtr<StringImpl>&& name, Ref<DeferredPromise>&& promise)
{
    getShared(GetType::GetAll, CookieStoreGetOptions { WTF::move(name), { } }, WTF::move(promise));
}

void CookieStore::getAll(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
{
    getShared(GetType::GetAll, WTF::move(options), WTF::move(promise));
}

void CookieStore::getShared(GetType getType, CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
{
    RefPtr context = scriptExecutionContext();
    if (!context) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    RefPtr origin = context->securityOrigin();
    if (!origin) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_si });
        return;
    }

    if (getType == GetType::Get && !options.name && !options.url) {
        promise->reject(Exception { ExceptionCode::TypeError, "CookieStoreGetOptions must not be empty"_si });
        return;
    }

    auto url = context->cookieURL();
    if (options.url) {
        auto parsed = context->parseURL(options.url);
        if (context->isDocument() && !equalIgnoringFragmentIdentifier(parsed, url)) {
            promise->reject(Exception { ExceptionCode::TypeError, "URL must match the document URL"_si });
            return;
        }

        if (!origin->isSameOriginAs(SecurityOrigin::create(parsed))) {
            promise->reject(Exception { ExceptionCode::TypeError, "Origin must match the context's origin"_si });
            return;
        }
        url = WTF::move(parsed);
        options.url = nullString();
    }

    if (options.name)
        options.name = CookieUtil::cookieStringForStorage(normalize(options.name));

    m_promises.add(++m_nextPromiseIdentifier, WTF::move(promise));
    auto completionHandler = [promiseIdentifier = m_nextPromiseIdentifier, getType](CookieStore& cookieStore, ExceptionOr<Vector<Cookie>>&& result) {
        auto promise = cookieStore.takePromise(promiseIdentifier);
        if (!promise)
            return;

        if (result.hasException()) {
            promise->reject(result.releaseException());
            return;
        }

        auto cookies = result.releaseReturnValue();

        if (getType == GetType::Get) {
            if (cookies.isEmpty()) {
                promise->resolveWithJSValue(JSC::jsNull());
                return;
            }

            promise->resolve<IDLDictionary<CookieListItem>>(CookieListItem::fromCookie(WTF::move(cookies[0])));
        } else {
            promise->resolve<IDLSequence<IDLDictionary<CookieListItem>>>(WTF::map(WTF::move(cookies), [](Cookie&& cookie) {
                return CookieListItem::fromCookie(WTF::move(cookie));
            }));
        }
    };

    m_mainThreadBridge->get(WTF::move(options), WTF::move(url), WTF::move(completionHandler));
}

void CookieStore::set(RefPtr<StringImpl>&& name, RefPtr<StringImpl>&& value, Ref<DeferredPromise>&& promise)
{
    set(CookieInit { WTF::move(name), WTF::move(value) }, WTF::move(promise));
}

void CookieStore::set(CookieInit&& options, Ref<DeferredPromise>&& promise)
{
    RefPtr context = scriptExecutionContext();
    if (!context) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    RefPtr origin = context->securityOrigin();
    if (!origin) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_si });
        return;
    }

    // https://cookiestore.spec.whatwg.org/#cookie-maximum-name-value-pair-size
    static constexpr auto maximumNameValuePairSize = 4096;
    // https://cookiestore.spec.whatwg.org/#cookie-maximum-attribute-value-size
    static constexpr auto maximumAttributeValueSize = 1024;

    auto url = context->cookieURL();
    auto domain = origin->domain();

    Cookie cookie;
    cookie.created = WallTime::now().secondsSinceEpoch().milliseconds();

    cookie.name = normalize(options.name);
    cookie.value = normalize(options.value);

    if (containsInvalidCharacters(cookie.name)) {
        promise->reject(Exception { ExceptionCode::TypeError, "The cookie name must not contain '\u003B', '\u007F', or any C0 control character except '\u0009'."_si });
        return;
    }

    if (containsInvalidCharacters(cookie.value)) {
        promise->reject(Exception { ExceptionCode::TypeError, "The cookie value must not contain '\u003B', '\u007F', or any C0 control character except '\u0009'."_si });
        return;
    }

    if (isEmpty(cookie.name)) {
        if (contains(cookie.value, '=')) {
            promise->reject(Exception { ExceptionCode::TypeError, "The cookie name and value must not both be set from the 'value' field."_si });
            return;
        }

        if (isEmpty(cookie.value)) {
            promise->reject(Exception { ExceptionCode::TypeError, "The cookie name and value must not both be empty."_si });
            return;
        }

        if (startsWithIgnoringASCIICase(cookie.value, "__Host-"_s)
            || startsWithIgnoringASCIICase(cookie.value, "__Host-Http-"_s)
            || startsWithIgnoringASCIICase(cookie.value, "__Http-"_s)
            || startsWithIgnoringASCIICase(cookie.value, "__Secure-"_s)) {
            promise->reject(Exception { ExceptionCode::TypeError, "If the cookie name is empty, the value must not begin with \"__Host-\", \"__Host-Http-\", \"__Http-\", or \"__Secure-\""_si });
            return;
        }
    }

    if (startsWithIgnoringASCIICase(cookie.name, "__Host-Http-"_s)
        || startsWithIgnoringASCIICase(cookie.name, "__Http-"_s)) {
            promise->reject(Exception { ExceptionCode::TypeError, "The cookie name must not begin with \"__Host-Http-\" or \"__Http-\""_si });
            return;
    }

    // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
    if (utf8(cookie.name).length() + utf8(cookie.value).length() > maximumNameValuePairSize) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("The size of the cookie name and value must not be greater than "_s, maximumNameValuePairSize, " bytes"_s) });
        return;
    }

    // With all checks complete, reinterpret the string, if necessary, before writing to storage
    cookie.name = CookieUtil::cookieStringForStorage(cookie.name);
    cookie.value = CookieUtil::cookieStringForStorage(cookie.value);

    // FIXME: This should be further down.
    if (!!options.domain && startsWithIgnoringASCIICase(cookie.name, "__Host-"_s)) {
        promise->reject(Exception { ExceptionCode::TypeError, "If the cookie name begins with \"__Host-\", the domain must not be specified."_si });
        return;
    }

    // FIXME: The specification does not perform this initialization of domain.
    cookie.domain = !options.domain ? domain : options.domain;
    if (cookie.domain) {
        if (startsWith(cookie.domain, '.')) {
            promise->reject(Exception { ExceptionCode::TypeError, "The domain must not begin with a '.'"_si });
            return;
        }

        if (!SecurityOrigin::create(url)->isMatchingRegistrableDomainSuffix(cookie.domain)) {
            promise->reject(Exception { ExceptionCode::TypeError, "The domain must be a registrable domain suffix of or be equal to the current host"_si });
            return;
        }

        // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
        if (utf8(cookie.domain).length() > maximumAttributeValueSize) {
            promise->reject(Exception { ExceptionCode::TypeError, makeString("The size of the domain must not be greater than "_s, maximumAttributeValueSize, " bytes"_s) });
            return;
        }

        // In CFNetwork, a domain without a leading dot means host-only cookie.
        // If a non-null domain was passed in, prepend dot to domain to set
        // host-only to false and make the cookie accessible by subdomains.
        if (options.domain)
            cookie.domain = makeString('.', cookie.domain);
    }

    cookie.path = WTF::move(options.path);
    ASSERT(cookie.path);
    if (isEmpty(cookie.path))
        cookie.path = CookieUtil::defaultPathForURL(url);

    if (!startsWith(cookie.path, '/')) {
        promise->reject(Exception { ExceptionCode::TypeError, "The path must begin with a '/'"_si });
        return;
    }

    if (!equal(cookie.path, "/"_s) && startsWithIgnoringASCIICase(cookie.name, "__Host-"_s)) {
        promise->reject(Exception { ExceptionCode::TypeError, "If the cookie name begins with \"__Host-\", the path must be \"/\" or default to that."_si });
        return;
    }

    // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
    if (utf8(cookie.path).length() > maximumAttributeValueSize) {
        promise->reject(Exception { ExceptionCode::TypeError, makeString("The size of the path must not be greater than "_s, maximumAttributeValueSize, " bytes"_s) });
        return;
    }

    if (options.expires) {
        if (options.maxAge) {
            promise->reject(Exception { ExceptionCode::TypeError, "Only one of 'expires' or 'maxAge' may be specified"_si });
            return;
        }

        // When this cookie is converted to an NSHTTPCookie, the creation and expiration
        // times will first be converted from milliseconds to seconds and then CFNetwork will
        // floor these times. If the creation and expiration differ by less than 1 second,
        // flooring them may reduce the difference to 0 seconds. This can cause the onchange
        // event to wrongly fire as a deletion instead of a change. In such cases, account for
        // this flooring by adding 1 second to the expiration.

        auto expires = *options.expires;

        auto expiresConverted = floor(Seconds::fromMilliseconds(expires).value());
        auto createdConverted = floor(Seconds::fromMilliseconds(cookie.created).value());
        bool equalAfterConversion = expiresConverted == createdConverted;

        if (equalAfterConversion && (expires > cookie.created))
            expires += Seconds(1).milliseconds();

        cookie.expires = expires;
    } else if (options.maxAge)
        cookie.expires = cookie.created + Seconds(*options.maxAge).milliseconds();

    switch (options.sameSite) {
    case CookieSameSite::Strict:
        cookie.sameSite = Cookie::SameSitePolicy::Strict;
        break;
    case CookieSameSite::Lax:
        cookie.sameSite = Cookie::SameSitePolicy::Lax;
        break;
    case CookieSameSite::None:
        cookie.sameSite = Cookie::SameSitePolicy::None;
        break;
    }

    cookie.secure = true;

    m_promises.add(++m_nextPromiseIdentifier, WTF::move(promise));
    auto completionHandler = [promiseIdentifier = m_nextPromiseIdentifier](CookieStore& cookieStore, std::optional<Exception>&& result) {
        auto promise = cookieStore.takePromise(promiseIdentifier);
        if (!promise)
            return;

        if (result)
            promise->reject(*result);
        else
            promise->resolve();
    };

    m_mainThreadBridge->set(WTF::move(cookie), WTF::move(url), WTF::move(completionHandler));
}

void CookieStore::remove(RefPtr<StringImpl>&& name, Ref<DeferredPromise>&& promise)
{
    remove(CookieStoreDeleteOptions { WTF::move(name), { } }, WTF::move(promise));
}

void CookieStore::remove(CookieStoreDeleteOptions&& options, Ref<DeferredPromise>&& promise)
{
    RefPtr context = scriptExecutionContext();
    if (!context) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    RefPtr origin = context->securityOrigin();
    if (!origin) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    if (origin->isOpaque()) {
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_si });
        return;
    }

    CookieInit initOptions;
    initOptions.name = WTF::move(options.name);
    initOptions.value = emptyString();
    initOptions.domain = WTF::move(options.domain);
    initOptions.path = WTF::move(options.path);
    initOptions.expires = (WallTime::now() - 24_h).secondsSinceEpoch().milliseconds();

    set(WTF::move(initOptions), WTF::move(promise));
}

void CookieStore::cookiesAdded(StringImpl* host, const Vector<Cookie>& cookies)
{
    ASSERT(m_hasChangeEventListener);

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    ASSERT_UNUSED(host, equal(host, downcast<Document>(context)->url().host().toString()));

    Vector<CookieListItem> deleted;
    Vector<CookieListItem> changed;
    for (auto cookie : cookies) {
        if (cookie.expires && *cookie.expires <= cookie.created) {
            cookie.value = nullString();
            deleted.append(CookieListItem::fromCookie(WTF::move(cookie)));
        } else
            changed.append(CookieListItem::fromCookie(WTF::move(cookie)));
    }

    auto eventInit = CookieChangeEvent::Init {
        { false, false, false },
        WTF::move(changed),
        WTF::move(deleted),
    };

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, CookieChangeEvent::create(eventNames().changeEvent, WTF::move(eventInit), CookieChangeEvent::IsTrusted::Yes));
}

void CookieStore::cookiesDeleted(StringImpl* host, const Vector<Cookie>& cookies)
{
    ASSERT(m_hasChangeEventListener);

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    ASSERT_UNUSED(host, equal(host, downcast<Document>(context)->url().host().toString()));

    CookieChangeEventInit eventInit;
    eventInit.deleted = cookies.map([](auto cookie) {
        cookie.value = nullString();
        return CookieListItem::fromCookie(WTF::move(cookie));
    });

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, CookieChangeEvent::create(eventNames().changeEvent, WTF::move(eventInit), CookieChangeEvent::IsTrusted::Yes));
}

void CookieStore::stop()
{
    // FIXME: This should work for service worker contexts as well.
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document)
        return;

    if (!m_hasChangeEventListener)
        return;

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (RefPtr cookieJar = m_cookieJar)
        cookieJar->removeChangeListener(m_host, *this);
#endif
    m_hasChangeEventListener = false;
    m_host = nullptr;
    m_cookieJar = nullptr;
}

bool CookieStore::virtualHasPendingActivity() const
{
    return m_hasChangeEventListener;
}

enum EventTargetInterfaceType CookieStore::eventTargetInterface() const
{
    return EventTargetInterfaceType::CookieStore;
}

ScriptExecutionContext* CookieStore::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void CookieStore::eventListenersDidChange()
{
    // FIXME: This should work for service worker contexts as well.
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document)
        return;

    bool hadChangeEventListener = m_hasChangeEventListener;
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);

    if (hadChangeEventListener == m_hasChangeEventListener)
        return;

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    if (m_hasChangeEventListener) {
        auto host = document->url().host().toString();
        RefPtr page = document->page();
        if (isEmpty(host) || !page) {
            m_hasChangeEventListener = false;
            return;
        }

        Ref cookieJar = page->cookieJar();
        cookieJar->addChangeListener(*document, *this);
        m_host = WTF::move(host);
        m_cookieJar = cookieJar;
    } else {
        if (RefPtr cookieJar = m_cookieJar)
            cookieJar->removeChangeListener(m_host, *this);
        m_host = nullptr;
        m_cookieJar = nullptr;
    }
#endif
}

RefPtr<DeferredPromise> CookieStore::takePromise(uint64_t promiseIdentifier)
{
    return m_promises.take(promiseIdentifier);
}

}
