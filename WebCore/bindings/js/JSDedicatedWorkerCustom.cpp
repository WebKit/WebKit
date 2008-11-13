/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"

#if ENABLE(WORKERS)

#include "JSDedicatedWorker.h"

#include "DedicatedWorker.h"
#include "Document.h"
#include "JSDOMWindowCustom.h"
#include "JSEventListener.h"
#include "JSMessagePort.h"
#include "MessagePort.h"

using namespace JSC;

namespace WebCore {
    
void JSDedicatedWorker::mark()
{
    DOMObject::mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onmessage()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onclose()))
        listener->mark();

    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(m_impl->onerror()))
        listener->mark();
}

JSValue* JSDedicatedWorker::connect(ExecState* exec, const ArgList& args)
{
    DOMWindow* window = asJSDOMWindow(exec->lexicalGlobalObject())->impl();
    const UString& message = args.at(exec, 0)->toString(exec);

    return toJS(exec, impl()->connect(window->document(), message).get());
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
