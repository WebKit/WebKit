/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSEventTargetNode.h"

namespace WebCore {

using namespace KJS;

JSEventTargetNode::JSEventTargetNode(JSObject* prototype, Node* node)
    : JSNode(prototype, node)
{
}

bool JSEventTargetNode::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return m_base.getOwnPropertySlot<JSNode>(this, exec, propertyName, slot);
}

JSValue* JSEventTargetNode::getValueProperty(ExecState* exec, int token) const
{
    return m_base.getValueProperty(this, exec, token);
}

void JSEventTargetNode::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    m_base.put<JSNode>(this, exec, propertyName, value, attr);
}

void JSEventTargetNode::putValueProperty(ExecState* exec, int token, JSValue* value, int attr)
{
    m_base.putValueProperty(this, exec, token, value, attr);
}

void JSEventTargetNode::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func) const
{
    Frame* frame = impl()->document()->frame();
    if (frame)
        EventTargetNodeCast(impl())->setHTMLEventListener(eventType, KJS::Window::retrieveWindow(frame)->findOrCreateJSEventListener(func, true));
}

JSValue* JSEventTargetNode::getListener(const AtomicString& eventType) const
{
    EventListener* listener = EventTargetNodeCast(impl())->getHTMLEventListener(eventType);
    JSEventListener* jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();

    return jsNull();
}

void JSEventTargetNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

EventTargetNode* toEventTargetNode(JSValue* val)
{
    if (!val || !val->isObject(&JSEventTargetNode::info))
        return 0;

    return static_cast<EventTargetNode*>(static_cast<JSEventTargetNode*>(val)->impl());
}

} // namespace WebCore
