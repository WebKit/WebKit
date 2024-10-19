/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "Cookie.h"
#include "CookieChangeEvent.h"
#include "CookieChangeEventInit.h"
#include "CookieInit.h"
#include "CookieJar.h"
#include "CookieListItem.h"
#include "CookieStoreDeleteOptions.h"
#include "CookieStoreGetOptions.h"
#include "Document.h"
#include "EventNames.h"
#include "ExceptionOr.h"
#include "JSCookieListItem.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "ScriptExecutionContextIdentifier.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "TaskSource.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
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

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CookieStore);

class CookieStore::MainThreadBridge : public ThreadSafeRefCounted<MainThreadBridge, WTF::DestructionThread::Main> {
public:
    static Ref<MainThreadBridge> create(CookieStore& cookieStore)
    {
        return adoptRef(*new MainThreadBridge(cookieStore));
    }

    void get(CookieStoreGetOptions&&, URL&&, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&&);
    void getAll(CookieStoreGetOptions&&, URL&&, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&&);
    void set(CookieInit&& options, Cookie&&, URL&&, Function<void(CookieStore&, std::optional<Exception>&&)>&&);

    void detach() { m_cookieStore = nullptr; }

private:
    explicit MainThreadBridge(CookieStore&);

    void ensureOnMainThread(Function<void(ScriptExecutionContext&)>&&);
    void ensureOnContextThread(Function<void(CookieStore&)>&&);

    RefPtr<CookieStore> protectedCookieStore() const { return m_cookieStore.get(); }
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

    RefPtr context = protectedCookieStore()->scriptExecutionContext();
    if (!context)
        return;
    ASSERT(context->isContextThread());

    if (RefPtr document = dynamicDowncast<Document>(*context)) {
        task(*context);
        return;
    }

    downcast<WorkerGlobalScope>(*context).protectedThread()->workerLoaderProxy()->postTaskToLoader(WTFMove(task));
}

void CookieStore::MainThreadBridge::ensureOnContextThread(Function<void(CookieStore&)>&& task)
{
    ScriptExecutionContext::ensureOnContextThread(*m_contextIdentifier, [protectedThis = Ref { *this }, task = WTFMove(task)](auto&) {
        if (RefPtr cookieStore = protectedThis->m_cookieStore.get())
            task(*cookieStore);
    });
}

void CookieStore::MainThreadBridge::get(CookieStoreGetOptions&& options, URL&& url, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&& completionHandler)
{
    ASSERT(m_cookieStore);

    auto getCookies = [this, protectedThis = Ref { *this }, options = crossThreadCopy(WTFMove(options)), url = crossThreadCopy(WTFMove(url)), completionHandler = WTFMove(completionHandler)](ScriptExecutionContext& context) mutable {
        Ref document = downcast<Document>(context);
        WeakPtr page = document->page();
        if (!page) {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler)](CookieStore& cookieStore) mutable {
                completionHandler(cookieStore, Exception { ExceptionCode::SecurityError });
            });
            return;
        }

        Ref cookieJar = page->cookieJar();
        auto resultHandler = [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] (std::optional<Vector<Cookie>>&& cookies) mutable {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler), cookies = crossThreadCopy(WTFMove(cookies))](CookieStore& cookieStore) mutable {
                if (!cookies)
                    completionHandler(cookieStore, Exception { ExceptionCode::TypeError });
                else
                    completionHandler(cookieStore, WTFMove(*cookies));
            });
        };

        cookieJar->getCookiesAsync(document, url, options, WTFMove(resultHandler));
    };

    ensureOnMainThread(WTFMove(getCookies));
}

void CookieStore::MainThreadBridge::getAll(CookieStoreGetOptions&& options, URL&& url, Function<void(CookieStore&, ExceptionOr<Vector<Cookie>>&&)>&& completionHandler)
{
    ASSERT(m_cookieStore);

    auto getAllCookies = [this, protectedThis = Ref { *this }, options = crossThreadCopy(WTFMove(options)), url = crossThreadCopy(WTFMove(url)), completionHandler = WTFMove(completionHandler)](ScriptExecutionContext& context) mutable {
        Ref document = downcast<Document>(context);
        WeakPtr page = document->page();
        if (!page) {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler)](CookieStore& cookieStore) mutable {
                completionHandler(cookieStore, Exception { ExceptionCode::SecurityError });
            });
            return;
        }

        Ref cookieJar = page->cookieJar();
        auto resultHandler = [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] (std::optional<Vector<Cookie>>&& cookies) mutable {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler), cookies = crossThreadCopy(WTFMove(cookies))](CookieStore& cookieStore) mutable {
                if (!cookies)
                    completionHandler(cookieStore, Exception { ExceptionCode::TypeError });
                else
                    completionHandler(cookieStore, WTFMove(*cookies));
            });
        };

        cookieJar->getCookiesAsync(document, url, options, WTFMove(resultHandler));
    };

    ensureOnMainThread(WTFMove(getAllCookies));
}

void CookieStore::MainThreadBridge::set(CookieInit&& options, Cookie&& cookie, URL&& url, Function<void(CookieStore&, std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(m_cookieStore);

    auto setCookie = [this, protectedThis = Ref { *this }, options = crossThreadCopy(WTFMove(options)), cookie = crossThreadCopy(WTFMove(cookie)), url = crossThreadCopy(WTFMove(url)), completionHandler = WTFMove(completionHandler)](ScriptExecutionContext& context) mutable {
        Ref document = downcast<Document>(context);
        WeakPtr page = document->page();
        if (!page) {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler)](CookieStore& cookieStore) mutable {
                completionHandler(cookieStore, Exception { ExceptionCode::SecurityError });
            });
            return;
        }

        Ref cookieJar = page->cookieJar();
        auto resultHandler = [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] (bool setSuccessfully) mutable {
            ensureOnContextThread([completionHandler = WTFMove(completionHandler), setSuccessfully](CookieStore& cookieStore) mutable {
                if (!setSuccessfully)
                    completionHandler(cookieStore, Exception { ExceptionCode::TypeError });
                else
                    completionHandler(cookieStore, std::nullopt);
            });
        };

        document->invalidateDOMCookieCache();
        cookieJar->setCookieAsync(document, url, cookie, WTFMove(resultHandler));
    };

    ensureOnMainThread(WTFMove(setCookie));
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
    protectedMainThreadBridge()->detach();
}

static bool containsInvalidCharacters(const String& string)
{
    // The invalid characters are specified at https://wicg.github.io/cookie-store/#set-a-cookie.
    return string.contains([](UChar character) {
        return character == 0x003B || character == 0x007F || (character <= 0x001F && character != 0x0009);
    });
}

void CookieStore::get(String&& name, Ref<DeferredPromise>&& promise)
{
    get(CookieStoreGetOptions { WTFMove(name), { } }, WTFMove(promise));
}

void CookieStore::get(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
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
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_s });
        return;
    }

    if (options.name.isNull() && options.url.isNull()) {
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    auto url = context->cookieURL();
    if (!options.url.isNull()) {
        auto parsed = context->completeURL(options.url);
        if (context->isDocument() && parsed != url) {
            promise->reject(ExceptionCode::TypeError);
            return;
        }

        if (!origin->isSameOriginAs(SecurityOrigin::create(parsed))) {
            promise->reject(ExceptionCode::TypeError);
            return;
        }
        url = WTFMove(parsed);
        options.url = nullString();
    }

    m_promises.add(++m_nextPromiseIdentifier, WTFMove(promise));
    auto completionHandler = [promiseIdentifier = m_nextPromiseIdentifier](CookieStore& cookieStore, ExceptionOr<Vector<Cookie>>&& result) {
        auto promise = cookieStore.takePromise(promiseIdentifier);
        if (!promise)
            return;

        if (result.hasException()) {
            promise->reject(result.releaseException());
            return;
        }

        auto cookies = result.releaseReturnValue();
        if (cookies.isEmpty()) {
            promise->resolveWithJSValue(JSC::jsNull());
            return;
        }

        promise->resolve<IDLDictionary<CookieListItem>>(CookieListItem(WTFMove(cookies[0])));
    };

    protectedMainThreadBridge()->get(WTFMove(options), WTFMove(url), WTFMove(completionHandler));
}

void CookieStore::getAll(String&& name, Ref<DeferredPromise>&& promise)
{
    getAll(CookieStoreGetOptions { WTFMove(name), { } }, WTFMove(promise));
}

void CookieStore::getAll(CookieStoreGetOptions&& options, Ref<DeferredPromise>&& promise)
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
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_s });
        return;
    }

    auto url = context->cookieURL();
    if (!options.url.isNull()) {
        auto parsed = context->completeURL(options.url);
        if (context->isDocument() && parsed != url) {
            promise->reject(ExceptionCode::TypeError);
            return;
        }

        if (!origin->isSameOriginAs(SecurityOrigin::create(parsed))) {
            promise->reject(ExceptionCode::TypeError);
            return;
        }
        url = WTFMove(parsed);
        options.url = nullString();
    }

    m_promises.add(++m_nextPromiseIdentifier, WTFMove(promise));
    auto completionHandler = [promiseIdentifier = m_nextPromiseIdentifier](CookieStore& cookieStore, ExceptionOr<Vector<Cookie>>&& result) {
        auto promise = cookieStore.takePromise(promiseIdentifier);
        if (!promise)
            return;

        if (result.hasException()) {
            promise->reject(result.releaseException());
            return;
        }

        auto cookies = result.releaseReturnValue();
        promise->resolve<IDLSequence<IDLDictionary<CookieListItem>>>(WTF::map(WTFMove(cookies), [](Cookie&& cookie) {
            return CookieListItem { WTFMove(cookie) };
        }));
    };

    protectedMainThreadBridge()->getAll(WTFMove(options), WTFMove(url), WTFMove(completionHandler));
}

void CookieStore::set(String&& name, String&& value, Ref<DeferredPromise>&& promise)
{
    set(CookieInit { WTFMove(name), WTFMove(value) }, WTFMove(promise));
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
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_s });
        return;
    }

    // The maximum attribute value size is specified at https://wicg.github.io/cookie-store/#cookie-maximum-attribute-value-size.
    static constexpr auto maximumAttributeValueSize = 1024;

    auto url = context->cookieURL();
    auto host = url.host();
    auto domain = origin->domain();

    Cookie cookie;
    cookie.name = WTFMove(options.name);
    cookie.value = WTFMove(options.value);

    if (containsInvalidCharacters(cookie.name)) {
        promise->reject(Exception { ExceptionCode::TypeError, "The cookie name must not contain '\u003B', '\u007F', or any C0 control character except '\u0009'."_s });
        return;
    }

    if (containsInvalidCharacters(cookie.value)) {
        promise->reject(Exception { ExceptionCode::TypeError, "The cookie value must not contain '\u003B', '\u007F', or any C0 control character except '\u0009'."_s });
        return;
    }

    if (cookie.name.isEmpty()) {
        if (cookie.value.contains('=')) {
            promise->reject(Exception { ExceptionCode::TypeError, "The cookie name and value must not both be set from the 'value' field."_s });
            return;
        }

        if (cookie.value.isEmpty()) {
            promise->reject(Exception { ExceptionCode::TypeError, "The cookie name and value must not both be empty."_s });
            return;
        }
    }

    cookie.created = WallTime::now().secondsSinceEpoch().milliseconds();

    cookie.domain = options.domain.isNull() ? domain : WTFMove(options.domain);
    if (!cookie.domain.isNull()) {
        if (cookie.domain.startsWith('.')) {
            promise->reject(Exception { ExceptionCode::TypeError, "The domain must not begin with a '.'"_s });
            return;
        }

        if (!host.endsWith(cookie.domain) || (host.length() > cookie.domain.length() && !host.substring(0, host.length() - cookie.domain.length()).endsWith('.'))) {
            promise->reject(Exception { ExceptionCode::TypeError, "The domain must be a part of the current host"_s });
            return;
        }

        // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
        if (cookie.domain.utf8().length() > maximumAttributeValueSize) {
            promise->reject(Exception { ExceptionCode::TypeError, makeString("The size of the domain must not be greater than "_s, maximumAttributeValueSize, " bytes"_s) });
            return;
        }
    }

    cookie.path = WTFMove(options.path);
    if (!cookie.path.isNull()) {
        if (!cookie.path.startsWith('/')) {
            promise->reject(Exception { ExceptionCode::TypeError, "The path must begin with a '/'"_s });
            return;
        }

        if (!cookie.path.endsWith('/'))
            cookie.path = makeString(cookie.path, '/');

        // FIXME: <rdar://85515842> Obtain the encoded length without allocating and encoding.
        if (cookie.path.utf8().length() > maximumAttributeValueSize) {
            promise->reject(Exception { ExceptionCode::TypeError, makeString("The size of the path must not be greater than "_s, maximumAttributeValueSize, " bytes"_s) });
            return;
        }
    }

    if (options.expires)
        cookie.expires = *options.expires;

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

    m_promises.add(++m_nextPromiseIdentifier, WTFMove(promise));
    auto completionHandler = [promiseIdentifier = m_nextPromiseIdentifier](CookieStore& cookieStore, std::optional<Exception>&& result) {
        auto promise = cookieStore.takePromise(promiseIdentifier);
        if (!promise)
            return;

        if (result)
            promise->reject(*result);
        else
            promise->resolve();
    };

    protectedMainThreadBridge()->set(WTFMove(options), WTFMove(cookie), WTFMove(url), WTFMove(completionHandler));
}

void CookieStore::remove(String&& name, Ref<DeferredPromise>&& promise)
{
    remove(CookieStoreDeleteOptions { WTFMove(name), { } }, WTFMove(promise));
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
        promise->reject(Exception { ExceptionCode::SecurityError, "The origin is opaque"_s });
        return;
    }

    CookieInit initOptions;
    initOptions.name = WTFMove(options.name);
    initOptions.value = emptyString();
    initOptions.domain = WTFMove(options.domain);
    initOptions.path = WTFMove(options.path);
    initOptions.expires = (WallTime::now() - 24_h).secondsSinceEpoch().milliseconds();

    set(WTFMove(initOptions), WTFMove(promise));
}

void CookieStore::cookiesAdded(const String& host, const Vector<Cookie>& cookies)
{
    ASSERT(m_hasChangeEventListener);

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    ASSERT_UNUSED(host, host == downcast<Document>(context)->url().host().toString());

    CookieChangeEventInit eventInit;
    auto currentTime = WallTime::now().secondsSinceEpoch().milliseconds();
    for (auto cookie : cookies) {
        if (cookie.expires && *cookie.expires < currentTime) {
            cookie.value = nullString();
            eventInit.deleted.append(CookieListItem { WTFMove(cookie) });
        } else
            eventInit.changed.append(CookieListItem { WTFMove(cookie) });
    }

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, CookieChangeEvent::create(eventNames().changeEvent, WTFMove(eventInit), CookieChangeEvent::IsTrusted::Yes));
}

void CookieStore::cookiesDeleted(const String& host, const Vector<Cookie>& cookies)
{
    ASSERT(m_hasChangeEventListener);

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    ASSERT_UNUSED(host, host == downcast<Document>(context)->url().host().toString());

    CookieChangeEventInit eventInit;
    eventInit.deleted = cookies.map([](auto cookie) {
        cookie.value = nullString();
        return CookieListItem { WTFMove(cookie) };
    });

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, CookieChangeEvent::create(eventNames().changeEvent, WTFMove(eventInit), CookieChangeEvent::IsTrusted::Yes));
}

void CookieStore::stop()
{
    // FIXME: This should work for service worker contexts as well.
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document)
        return;

    if (!m_hasChangeEventListener)
        return;

    WeakPtr page = document->page();
    if (!page)
        return;

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    auto host = document->url().host().toString();
    page->protectedCookieJar()->removeChangeListener(host, *this);
#endif
    m_hasChangeEventListener = false;
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
    if (!is<Document>(scriptExecutionContext()))
        return;

    bool hadChangeEventListener = m_hasChangeEventListener;
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);

    if (hadChangeEventListener == m_hasChangeEventListener)
        return;

    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return;

    WeakPtr page = document->page();
    if (!page)
        return;

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    Ref cookieJar = page->cookieJar();
    auto host = document->url().host().toString();
    if (m_hasChangeEventListener)
        cookieJar->addChangeListener(host, *this);
    else
        cookieJar->removeChangeListener(host, *this);
#endif
}

RefPtr<DeferredPromise> CookieStore::takePromise(uint64_t promiseIdentifier)
{
    return m_promises.take(promiseIdentifier);
}

Ref<CookieStore::MainThreadBridge> CookieStore::protectedMainThreadBridge() const
{
    return m_mainThreadBridge;
}

}
