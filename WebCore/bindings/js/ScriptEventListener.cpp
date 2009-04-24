/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptEventListener.h"

#include "Attribute.h"
#include "Document.h"
#include "JSNode.h"
#include "Frame.h"

#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

static const String& eventParameterName(bool isSVGEvent)
{
    DEFINE_STATIC_LOCAL(const String, eventString, ("event"));
    DEFINE_STATIC_LOCAL(const String, evtString, ("evt"));
    return isSVGEvent ? evtString : eventString;
}

PassRefPtr<JSLazyEventListener> createAttributeEventListener(Node* node, Attribute* attr)
{
    ASSERT(node);

    Frame* frame = node->document()->frame();
    if (!frame)
        return 0;

    ScriptController* scriptController = frame->script();
    if (!scriptController->isEnabled())
        return 0;

    JSDOMWindow* globalObject = scriptController->globalObject();

    // Ensure that 'node' has a JavaScript wrapper to mark the event listener we're creating.
    {
        JSLock lock(false);
        toJS(globalObject->globalExec(), node);
    }

    return JSLazyEventListener::create(attr->localName().string(), eventParameterName(node->isSVGElement()), attr->value(), globalObject, node, scriptController->eventHandlerLineNumber());
}

PassRefPtr<JSLazyEventListener> createAttributeEventListener(Frame* frame, Attribute* attr)
{
    if (!frame)
        return 0;

    ScriptController* scriptController = frame->script();
    if (!scriptController->isEnabled())
        return 0;

    // 'globalObject' is the JavaScript wrapper that will mark the event listener we're creating.
    JSDOMWindow* globalObject = scriptController->globalObject();

    return JSLazyEventListener::create(attr->localName().string(), eventParameterName(frame->document()->isSVGDocument()), attr->value(), globalObject, 0, scriptController->eventHandlerLineNumber());
}

} // namespace WebCore
