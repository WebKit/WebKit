/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "JSDOMPromise.h"
#include "MediaKeySessionType.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class BufferSource;
class MediaKeyStatusMap;
class MediaKeys;

class MediaKeySession final : public RefCounted<MediaKeySession>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<MediaKeySession> create(ScriptExecutionContext&);
    virtual ~MediaKeySession();

    using RefCounted<MediaKeySession>::ref;
    using RefCounted<MediaKeySession>::deref;

    const String& sessionId() const;
    double expiration() const;
    RefPtr<MediaKeyStatusMap> keyStatuses() const;

    void generateRequest(const String&, const BufferSource&, Ref<DeferredPromise>&&);
    void load(const String&, Ref<DeferredPromise>&&);
    void update(const BufferSource&, Ref<DeferredPromise>&&);
    void close(Ref<DeferredPromise>&&);
    void remove(Ref<DeferredPromise>&&);

private:
    MediaKeySession(ScriptExecutionContext&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return MediaKeySessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject
    bool hasPendingActivity() const override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
