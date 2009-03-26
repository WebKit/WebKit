/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSEventListener.h"

#include "Event.h"
#include "Frame.h"
#include "JSEvent.h"
#include "JSEventTarget.h"
#include <runtime/JSLock.h>
#include <wtf/RefCountedLeakCounter.h>

using namespace JSC;

namespace WebCore {

void JSAbstractEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    JSLock lock(false);

    JSObject* listener = function();
    if (!listener)
        return;

    JSDOMGlobalObject* globalObject = this->globalObject();
    // Null check as clearGlobalObject() can clear this and we still get called back by
    // xmlhttprequest objects. See http://bugs.webkit.org/show_bug.cgi?id=13275
    // FIXME: Is this check still necessary? Requests are supposed to be stopped before clearGlobalObject() is called.
    if (!globalObject)
        return;

    ScriptExecutionContext* scriptExecutionContext = globalObject->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    if (scriptExecutionContext->isDocument()) {
        JSDOMWindow* window = static_cast<JSDOMWindow*>(globalObject);
        Frame* frame = window->impl()->frame();
        if (!frame)
            return;
        // The window must still be active in its frame. See <https://bugs.webkit.org/show_bug.cgi?id=21921>.
        // FIXME: A better fix for this may be to change DOMWindow::frame() to not return a frame the detached window used to be in.
        if (frame->domWindow() != window->impl())
            return;
        // FIXME: Is this check needed for other contexts?
        ScriptController* script = frame->script();
        if (!script->isEnabled() || script->isPaused())
            return;
    }

    ExecState* exec = globalObject->globalExec();

    JSValuePtr handleEventFunction = listener->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = handleEventFunction.getCallData(callData);
    if (callType == CallTypeNone) {
        handleEventFunction = noValue();
        callType = listener->getCallData(callData);
    }

    if (callType != CallTypeNone) {
        ref();

        ArgList args;
        args.append(toJS(exec, event));

        Event* savedEvent = globalObject->currentEvent();
        globalObject->setCurrentEvent(event);

        // If this event handler is the first JavaScript to execute, then the
        // dynamic global object should be set to the global object of the
        // window in which the event occurred.
        JSGlobalData* globalData = globalObject->globalData();
        DynamicGlobalObjectScope globalObjectScope(exec, globalData->dynamicGlobalObject ? globalData->dynamicGlobalObject : globalObject);

        JSValuePtr retval;
        if (handleEventFunction) {
            globalObject->globalData()->timeoutChecker.start();
            retval = call(exec, handleEventFunction, callType, callData, listener, args);
        } else {
            JSValuePtr thisValue;
            if (isWindowEvent)
                thisValue = globalObject->toThisObject(exec);
            else
                thisValue = toJS(exec, event->currentTarget());
            globalObject->globalData()->timeoutChecker.start();
            retval = call(exec, listener, callType, callData, thisValue, args);
        }
        globalObject->globalData()->timeoutChecker.stop();

        globalObject->setCurrentEvent(savedEvent);

        if (exec->hadException())
            reportCurrentException(exec);
        else {
            if (!retval.isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval.toString(exec));
            if (m_isInline) {
                bool retvalbool;
                if (retval.getBoolean(retvalbool) && !retvalbool)
                    event->preventDefault();
            }
        }

        if (scriptExecutionContext->isDocument())
            Document::updateDocumentsRendering();
        deref();
    }
}

bool JSAbstractEventListener::virtualIsInline() const
{
    return m_isInline;
}

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
    : JSAbstractEventListener(isInline)
    , m_listener(listener)
    , m_globalObject(globalObject)
{
    if (m_listener) {
        JSDOMWindow::JSListenersMap& listeners = isInline
            ? globalObject->jsInlineEventListeners() : globalObject->jsEventListeners();
        listeners.set(m_listener, this);
    }
}

JSEventListener::~JSEventListener()
{
    if (m_listener && m_globalObject) {
        JSDOMWindow::JSListenersMap& listeners = isInline()
            ? m_globalObject->jsInlineEventListeners() : m_globalObject->jsEventListeners();
        listeners.remove(m_listener);
    }
}

JSObject* JSEventListener::function() const
{
    return m_listener;
}

JSDOMGlobalObject* JSEventListener::globalObject() const
{
    return m_globalObject;
}

void JSEventListener::clearGlobalObject()
{
    m_globalObject = 0;
}

void JSEventListener::mark()
{
    if (m_listener && !m_listener->marked())
        m_listener->mark();
}

#ifndef NDEBUG
static WTF::RefCountedLeakCounter eventListenerCounter("EventListener");
#endif

// -------------------------------------------------------------------------

JSProtectedEventListener::JSProtectedEventListener(JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
    : JSAbstractEventListener(isInline)
    , m_listener(listener)
    , m_globalObject(globalObject)
{
    if (m_listener) {
        JSDOMWindow::ProtectedListenersMap& listeners = isInline
            ? m_globalObject->jsProtectedInlineEventListeners() : m_globalObject->jsProtectedEventListeners();
        listeners.set(m_listener, this);
    }
#ifndef NDEBUG
    eventListenerCounter.increment();
#endif
}

JSProtectedEventListener::~JSProtectedEventListener()
{
    if (m_listener && m_globalObject) {
        JSDOMWindow::ProtectedListenersMap& listeners = isInline()
            ? m_globalObject->jsProtectedInlineEventListeners() : m_globalObject->jsProtectedEventListeners();
        listeners.remove(m_listener);
    }
#ifndef NDEBUG
    eventListenerCounter.decrement();
#endif
}

JSObject* JSProtectedEventListener::function() const
{
    return m_listener;
}

JSDOMGlobalObject* JSProtectedEventListener::globalObject() const
{
    return m_globalObject;
}

void JSProtectedEventListener::clearGlobalObject()
{
    m_globalObject = 0;
}

} // namespace WebCore
