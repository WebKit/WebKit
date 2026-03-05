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

#pragma once

#if ENABLE(USER_MESSAGE_HANDLERS)

#include <WebCore/FrameDestructionObserver.h>
#include <WebCore/UserMessageHandlerDescriptor.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class DeferredPromise;
template<typename> class ExceptionOr;

class UserMessageHandler : public RefCounted<UserMessageHandler>, public FrameDestructionObserver {
public:
    static Ref<UserMessageHandler> create(LocalFrame& frame, const UserMessageHandlerDescriptor& descriptor)
    {
        return adoptRef(*new UserMessageHandler(frame, descriptor));
    }
    virtual ~UserMessageHandler();

    // FrameDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    ExceptionOr<void> postMessage(JSC::JSGlobalObject&, JSC::JSValue, Ref<DeferredPromise>&&);
    JSC::JSValue postLegacySynchronousMessage(JSC::JSGlobalObject&, JSC::JSValue);

    const UserMessageHandlerDescriptor* descriptor() { return m_descriptor.get(); }
    void invalidateDescriptor() { m_descriptor = nullptr; }

private:
    UserMessageHandler(LocalFrame&, const UserMessageHandlerDescriptor&);
    
    RefPtr<const UserMessageHandlerDescriptor> m_descriptor;
};

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
