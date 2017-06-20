/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "Event.h"
#include <heap/Strong.h>
#include <runtime/JSPromise.h>

namespace WebCore {

class PromiseRejectionEvent final : public Event {
public:
    struct Init : EventInit {
        JSC::JSPromise* promise;
        JSC::JSValue reason;
    };

    static Ref<PromiseRejectionEvent> create(JSC::ExecState& state, const AtomicString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new PromiseRejectionEvent(state, type, initializer, isTrusted));
    }

    virtual ~PromiseRejectionEvent();

    JSC::JSPromise& promise() const { return *m_promise.get(); }
    JSC::JSValue reason() const { return m_reason.get(); }

    EventInterface eventInterface() const override { return PromiseRejectionEventInterfaceType; }

private:
    PromiseRejectionEvent(JSC::ExecState&, const AtomicString&, const Init&, IsTrusted);

    JSC::Strong<JSC::JSPromise> m_promise;
    JSC::Strong<JSC::Unknown> m_reason;
};

} // namespace WebCore
