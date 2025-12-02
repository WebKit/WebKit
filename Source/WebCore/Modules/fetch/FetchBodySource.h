/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ReadableStreamSource.h"
#include <WebCore/ActiveDOMObject.h>

namespace JSC {
class ArrayBuffer;
};

namespace WebCore {

class DOMPromise;
class FetchBodyOwner;
class ReadableByteStreamController;

class FetchBodySource final : public RefCounted<FetchBodySource> {
public:
    static std::pair<Ref<FetchBodySource>, Ref<RefCountedReadableStreamSource>> createNonByteSource(FetchBodyOwner&);
    static Ref<FetchBodySource> createByteSource(FetchBodyOwner&);

    ~FetchBodySource();

    bool enqueue(RefPtr<JSC::ArrayBuffer>&&);
    void close();
    void error(const Exception&);

    bool isPulling() const;
    bool isCancelling() const;

    void resolvePullPromise();
    void detach();

    void setByteController(ReadableByteStreamController&);
    Ref<DOMPromise> pull(JSDOMGlobalObject&, ReadableByteStreamController&);
    Ref<DOMPromise> cancel(JSDOMGlobalObject&, ReadableByteStreamController&, std::optional<JSC::JSValue>&&);

private:
    class NonByteSource;
    FetchBodySource(FetchBodyOwner&, RefPtr<NonByteSource>&& = { });

    class NonByteSource : public RefCountedReadableStreamSource {
    public:
        static Ref<NonByteSource> create(FetchBodyOwner& owner) { return adoptRef(*new NonByteSource(owner)); }

        bool enqueue(RefPtr<JSC::ArrayBuffer>&& chunk) { return controller().enqueue(WTFMove(chunk)); }
        void close();
        void error(const Exception&);

        bool isCancelling() const { return m_isCancelling; }

        void resolvePullPromise() { pullFinished(); }
        void detach() { m_bodyOwner = nullptr; }

    private:
        explicit NonByteSource(FetchBodyOwner&);

        void doStart() final;
        void doPull() final;
        void doCancel(JSC::JSValue) final;
        void setActive() final;
        void setInactive() final;

        WeakPtr<FetchBodyOwner> m_bodyOwner;

        bool m_isCancelling { false };
#if ASSERT_ENABLED
        bool m_isClosed { false };
#endif
        RefPtr<ActiveDOMObject::PendingActivity<FetchBodyOwner>> m_pendingActivity;
    };

    WeakPtr<FetchBodyOwner> m_bodyOwner;
    bool m_isCancelling { false };
    bool m_isPulling { false };

    const RefPtr<NonByteSource> m_nonByteSource;

    WeakPtr<ReadableByteStreamController> m_byteController;
    RefPtr<DeferredPromise> m_pullPromise;
};

} // namespace WebCore
