/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/ExceptionOr.h>
#include <WebCore/JSDOMGuardedObject.h>

namespace WebCore {

class DOMAsyncIterator final : public DOMGuardedObject {
public:
    WEBCORE_EXPORT static ExceptionOr<Ref<DOMAsyncIterator>> create(JSDOMGlobalObject&, JSC::JSValue /* iterable */);
    using Callback = Function<void(JSDOMGlobalObject*, bool, JSC::JSValue)>;
    WEBCORE_EXPORT void callNext(Callback&&);
    WEBCORE_EXPORT void callReturn(JSC::JSValue reason, Callback&&);

private:
    class IteratorObject final : public DOMGuarded<JSC::JSObject> {
    public:
        static Ref<IteratorObject> create(JSDOMGlobalObject& globalObject, JSC::JSObject& iterator) { return adoptRef(*new IteratorObject(globalObject, iterator)); }

        JSC::JSObject* object() const { return guarded(); }

    private:
        IteratorObject(JSDOMGlobalObject& globalObject, JSC::JSObject& iterator)
            : DOMGuarded<JSC::JSObject>(globalObject, iterator)
        {
        }
    };

    DOMAsyncIterator(JSDOMGlobalObject& globalObject, JSC::JSObject& iteratorObject, JSC::JSCell& method)
        : DOMGuardedObject(globalObject, method)
        , m_iterator(IteratorObject::create(globalObject, iteratorObject))
    {
    }

    void handleCallbackWithPromise(JSDOMGlobalObject&, Callback&&, JSC::JSPromise&);

    const Ref<IteratorObject> m_iterator;
    Callback m_callback;
};

} // namespace WebCore
