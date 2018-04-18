/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "JSDOMWindow.h"
#include <JavaScriptCore/JSProxy.h>

namespace JSC {
class Debugger;
}

namespace WebCore {

class AbstractDOMWindow;
class AbstractFrame;

class JSDOMWindowProxy final : public JSC::JSProxy {
    using Base = JSC::JSProxy;
public:
    static JSDOMWindowProxy& create(JSC::VM&, AbstractDOMWindow&, DOMWrapperWorld&);
    static void destroy(JSCell*);

    DECLARE_INFO;

    JSDOMGlobalObject* window() const { return JSC::jsCast<JSDOMGlobalObject*>(target()); }
    void setWindow(JSC::VM&, JSDOMGlobalObject&);
    void setWindow(AbstractDOMWindow&);

    AbstractDOMWindow& wrapped() const;
    static WEBCORE_EXPORT AbstractDOMWindow* toWrapped(JSC::VM&, JSC::JSObject*);

    DOMWrapperWorld& world() { return m_world; }

    void attachDebugger(JSC::Debugger*);

private:
    JSDOMWindowProxy(JSC::VM&, JSC::Structure&, DOMWrapperWorld&);
    void finishCreation(JSC::VM&, AbstractDOMWindow&);

    Ref<DOMWrapperWorld> m_world;
};

// JSDOMWindowProxy is a little odd in that it's not a traditional wrapper and has no back pointer.
// It is, however, strongly owned by Frame via its ScriptController, so we can get one from a frame.
JSC::JSValue toJS(JSC::ExecState*, AbstractFrame&);
inline JSC::JSValue toJS(JSC::ExecState* state, AbstractFrame* frame) { return frame ? toJS(state, *frame) : JSC::jsNull(); }

JSDOMWindowProxy& toJSDOMWindowProxy(AbstractFrame&, DOMWrapperWorld&);
inline JSDOMWindowProxy* toJSDOMWindowProxy(AbstractFrame* frame, DOMWrapperWorld& world) { return frame ? &toJSDOMWindowProxy(*frame, world) : nullptr; }

} // namespace WebCore
