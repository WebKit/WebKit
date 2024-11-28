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

#pragma once

#include "ActiveDOMObject.h"
#include "CookieChangeListener.h"
#include "CookieJar.h"
#include "EventTarget.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct CookieInit;
struct CookieStoreDeleteOptions;
struct CookieStoreGetOptions;
class Document;
class DeferredPromise;
class ScriptExecutionContext;

class CookieStore final : public RefCounted<CookieStore>, public EventTarget, public ActiveDOMObject, public CookieChangeListener {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(CookieStore);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    USING_CAN_MAKE_WEAKPTR(EventTarget);

    static Ref<CookieStore> create(ScriptExecutionContext*);
    ~CookieStore();

    void get(String&& name, Ref<DeferredPromise>&&);
    void get(CookieStoreGetOptions&&, Ref<DeferredPromise>&&);

    void getAll(String&& name, Ref<DeferredPromise>&&);
    void getAll(CookieStoreGetOptions&&, Ref<DeferredPromise>&&);

    void set(String&& name, String&& value, Ref<DeferredPromise>&&);
    void set(CookieInit&&, Ref<DeferredPromise>&&);

    void remove(String&& name, Ref<DeferredPromise>&&);
    void remove(CookieStoreDeleteOptions&&, Ref<DeferredPromise>&&);

private:
    explicit CookieStore(ScriptExecutionContext*);

    // CookieChangeListener
    void cookiesAdded(const String& host, const Vector<Cookie>&) final;
    void cookiesDeleted(const String& host, const Vector<Cookie>&) final;

    // ActiveDOMObject
    void stop() final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;

    RefPtr<DeferredPromise> takePromise(uint64_t promiseIdentifier);

    class MainThreadBridge;
    Ref<MainThreadBridge> m_mainThreadBridge;
    Ref<MainThreadBridge> protectedMainThreadBridge() const;

    bool m_hasChangeEventListener { false };
    WeakPtr<CookieJar> m_cookieJar;
    String m_host;
    uint64_t m_nextPromiseIdentifier { 0 };
    HashMap<uint64_t, Ref<DeferredPromise>> m_promises;
};

}
