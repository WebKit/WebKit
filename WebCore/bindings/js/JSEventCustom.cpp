/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "config.h"
#include "JSEvent.h"

#include "Clipboard.h"
#include "Event.h"
#include "JSClipboard.h"
#include "JSKeyboardEvent.h"
#include "JSMessageEvent.h"
#include "JSMouseEvent.h"
#include "JSMutationEvent.h"
#include "JSOverflowEvent.h"
#include "JSProgressEvent.h"
#include "JSTextEvent.h"
#include "JSUIEvent.h"
#include "JSWheelEvent.h"
#include "JSXMLHttpRequestProgressEvent.h"
#include "KeyboardEvent.h"
#include "MessageEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "OverflowEvent.h"
#include "ProgressEvent.h"
#include "TextEvent.h"
#include "UIEvent.h"
#include "WheelEvent.h"
#include "XMLHttpRequestProgressEvent.h"

#if ENABLE(DOM_STORAGE)
#include "JSStorageEvent.h"
#include "StorageEvent.h"
#endif

#if ENABLE(SVG)
#include "JSSVGZoomEvent.h"
#include "SVGZoomEvent.h"
#endif

using namespace KJS;

namespace WebCore {

JSValue* JSEvent::clipboardData(ExecState* exec) const
{
    return impl()->isClipboardEvent() ? toJS(exec, impl()->clipboardData()) : jsUndefined();
}

JSValue* toJS(ExecState* exec, Event* event)
{
    JSLock lock;

    if (!event)
        return jsNull();

    DOMObject* ret = ScriptInterpreter::getDOMObject(event);
    if (ret)
        return ret;

    if (event->isUIEvent()) {
        if (event->isKeyboardEvent())
            ret = new (exec) JSKeyboardEvent(JSKeyboardEventPrototype::self(exec), static_cast<KeyboardEvent*>(event));
        else if (event->isTextEvent())
            ret = new (exec) JSTextEvent(JSTextEventPrototype::self(exec), static_cast<TextEvent*>(event));
        else if (event->isMouseEvent())
            ret = new (exec) JSMouseEvent(JSMouseEventPrototype::self(exec), static_cast<MouseEvent*>(event));
        else if (event->isWheelEvent())
            ret = new (exec) JSWheelEvent(JSWheelEventPrototype::self(exec), static_cast<WheelEvent*>(event));
#if ENABLE(SVG)
        else if (event->isSVGZoomEvent())
            ret = new (exec) JSSVGZoomEvent(JSSVGZoomEventPrototype::self(exec), static_cast<SVGZoomEvent*>(event), 0);
#endif
        else
            ret = new (exec) JSUIEvent(JSUIEventPrototype::self(exec), static_cast<UIEvent*>(event));
    } else if (event->isMutationEvent())
        ret = new (exec) JSMutationEvent(JSMutationEventPrototype::self(exec), static_cast<MutationEvent*>(event));
    else if (event->isOverflowEvent())
        ret = new (exec) JSOverflowEvent(JSOverflowEventPrototype::self(exec), static_cast<OverflowEvent*>(event));
    else if (event->isMessageEvent())
        ret = new (exec) JSMessageEvent(JSMessageEventPrototype::self(exec), static_cast<MessageEvent*>(event));
    else if (event->isProgressEvent()) {
        if (event->isXMLHttpRequestProgressEvent())
            ret = new (exec) JSXMLHttpRequestProgressEvent(JSXMLHttpRequestProgressEventPrototype::self(exec), static_cast<XMLHttpRequestProgressEvent*>(event));
        else
            ret = new (exec) JSProgressEvent(JSProgressEventPrototype::self(exec), static_cast<ProgressEvent*>(event));
    }
#if ENABLE(DOM_STORAGE)
    else if (event->isStorageEvent())
        ret = new (exec) JSStorageEvent(JSStorageEventPrototype::self(exec), static_cast<StorageEvent*>(event));
#endif
    else
        ret = new (exec) JSEvent(JSEventPrototype::self(exec), event);

    ScriptInterpreter::putDOMObject(event, ret);
    return ret;
}

} // namespace WebCore
