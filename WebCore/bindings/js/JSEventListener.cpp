/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All Rights Reserved.
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

#include "CString.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "JSDOMWindow.h"
#include "JSEvent.h"
#include "JSEventTarget.h"
#include "JSEventTargetNode.h"
#include "ScriptController.h"
#include <runtime/FunctionConstructor.h>
#include <runtime/JSLock.h>
#include <wtf/RefCountedLeakCounter.h>

using namespace JSC;

namespace WebCore {

using namespace EventNames;

ASSERT_CLASS_FITS_IN_CELL(JSAbstractEventListener)

void JSAbstractEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    JSLock lock(false);

    JSObject* listener = listenerObj();
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

    Frame* frame = 0;
    if (scriptExecutionContext->isDocument()) {
        JSDOMWindow* window = static_cast<JSDOMWindow*>(globalObject);
        frame = window->impl()->frame();
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

    JSValue* handleEventFunction = listener->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = handleEventFunction->getCallData(callData);
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

        JSValue* retval;
        if (handleEventFunction) {
            globalObject->startTimeoutCheck();
            retval = call(exec, handleEventFunction, callType, callData, listener, args);
        } else {
            JSValue* thisValue;
            if (isWindowEvent)
                thisValue = globalObject->toThisObject(exec);
            else
                thisValue = toJS(exec, event->currentTarget());
            globalObject->startTimeoutCheck();
            retval = call(exec, listener, callType, callData, thisValue, args);
        }
        globalObject->stopTimeoutCheck();

        globalObject->setCurrentEvent(savedEvent);

        if (exec->hadException()) {
            // FIXME: Report exceptions in non-Document contexts.
            if (frame)
                frame->domWindow()->console()->reportCurrentException(exec);
        } else {
            if (!retval->isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval->toString(exec));
            if (m_isInline) {
                bool retvalbool;
                if (retval->getBoolean(retvalbool) && !retvalbool)
                    event->preventDefault();
            }
        }

        Document::updateDocumentsRendering();
        deref();
    }
}

bool JSAbstractEventListener::isInline() const
{
    return m_isInline;
}

// -------------------------------------------------------------------------

JSUnprotectedEventListener::JSUnprotectedEventListener(JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
    : JSAbstractEventListener(isInline)
    , m_listener(listener)
    , m_globalObject(globalObject)
{
    if (m_listener) {
        JSDOMWindow::UnprotectedListenersMap& listeners = isInline
            ? globalObject->jsUnprotectedInlineEventListeners() : globalObject->jsUnprotectedEventListeners();
        listeners.set(m_listener, this);
    }
}

JSUnprotectedEventListener::~JSUnprotectedEventListener()
{
    if (m_listener && m_globalObject) {
        JSDOMWindow::UnprotectedListenersMap& listeners = isInline()
            ? m_globalObject->jsUnprotectedInlineEventListeners() : m_globalObject->jsUnprotectedEventListeners();
        listeners.remove(m_listener);
    }
}

JSObject* JSUnprotectedEventListener::listenerObj() const
{
    return m_listener;
}

JSDOMGlobalObject* JSUnprotectedEventListener::globalObject() const
{
    return m_globalObject;
}

void JSUnprotectedEventListener::clearGlobalObject()
{
    m_globalObject = 0;
}

void JSUnprotectedEventListener::mark()
{
    if (m_listener && !m_listener->marked())
        m_listener->mark();
}

#ifndef NDEBUG
static WTF::RefCountedLeakCounter eventListenerCounter("EventListener");
#endif

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(JSObject* listener, JSDOMGlobalObject* globalObject, bool isInline)
    : JSAbstractEventListener(isInline)
    , m_listener(listener)
    , m_globalObject(globalObject)
{
    if (m_listener) {
        JSDOMWindow::ListenersMap& listeners = isInline
            ? m_globalObject->jsInlineEventListeners() : m_globalObject->jsEventListeners();
        listeners.set(m_listener, this);
    }
#ifndef NDEBUG
    eventListenerCounter.increment();
#endif
}

JSEventListener::~JSEventListener()
{
    if (m_listener && m_globalObject) {
        JSDOMWindow::ListenersMap& listeners = isInline()
            ? m_globalObject->jsInlineEventListeners() : m_globalObject->jsEventListeners();
        listeners.remove(m_listener);
    }
#ifndef NDEBUG
    eventListenerCounter.decrement();
#endif
}

JSObject* JSEventListener::listenerObj() const
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

// -------------------------------------------------------------------------

JSLazyEventListener::JSLazyEventListener(LazyEventListenerType type, const String& functionName, const String& code, JSDOMGlobalObject* globalObject, Node* node, int lineNumber)
    : JSEventListener(0, globalObject, true)
    , m_functionName(functionName)
    , m_code(code)
    , m_parsed(false)
    , m_lineNumber(lineNumber)
    , m_originalNode(node)
    , m_type(type)
{
    // We don't retain the original node because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, parseCode will be called and
    // then originalNode is no longer needed.

    // A JSLazyEventListener can be created with a line number of zero when it is created with
    // a setAttribute call from JavaScript, so make the line number 1 in that case.
    if (m_lineNumber == 0)
        m_lineNumber = 1;
}

JSObject* JSLazyEventListener::listenerObj() const
{
    parseCode();
    return m_listener;
}

// Helper function
inline JSValue* eventParameterName(JSLazyEventListener::LazyEventListenerType type, ExecState* exec)
{
    switch (type) {
    case JSLazyEventListener::HTMLLazyEventListener:
        return jsNontrivialString(exec, "event");
#if ENABLE(SVG)
    case JSLazyEventListener::SVGLazyEventListener:
        return jsNontrivialString(exec, "evt");
#endif
    default:
        ASSERT_NOT_REACHED();
        return jsUndefined();
    }
}

void JSLazyEventListener::parseCode() const
{
    if (m_parsed)
        return;

    Frame* frame = 0;
    if (globalObject()->scriptExecutionContext()->isDocument()) {
        JSDOMWindow* window = static_cast<JSDOMWindow*>(globalObject());
        frame = window->impl()->frame();
        if (!frame)
            return;
        // FIXME: Is this check needed for non-Document contexts?
        ScriptController* script = frame->script();
        if (!script->isEnabled() || script->isPaused())
            return;
    }

    m_parsed = true;

    ExecState* exec = globalObject()->globalExec();

    ArgList args;
    UString sourceURL(globalObject()->scriptExecutionContext()->url().string());
    args.append(eventParameterName(m_type, exec));
    args.append(jsString(exec, m_code));

    // FIXME: Passing the document's URL to construct is not always correct, since this event listener might
    // have been added with setAttribute from a script, and we should pass String() in that case.
    m_listener = constructFunction(exec, args, Identifier(exec, m_functionName), sourceURL, m_lineNumber); // FIXME: is globalExec ok?

    JSFunction* listenerAsFunction = static_cast<JSFunction*>(m_listener.get());

    if (exec->hadException()) {
        exec->clearException();

        // failed to parse, so let's just make this listener a no-op
        m_listener = 0;
    } else if (m_originalNode) {
        // Add the event's home element to the scope
        // (and the document, and the form - see JSHTMLElement::eventHandlerScope)
        ScopeChain scope = listenerAsFunction->scope();

        JSValue* thisObj = toJS(exec, m_originalNode);
        if (thisObj->isObject()) {
            static_cast<JSEventTargetNode*>(asObject(thisObj))->pushEventHandlerScope(exec, scope);
            listenerAsFunction->setScope(scope);
        }
    }

    // no more need to keep the unparsed code around
    m_functionName = String();
    m_code = String();

    if (m_listener) {
        ASSERT(isInline());
        JSDOMWindow::ListenersMap& listeners = globalObject()->jsInlineEventListeners();
        listeners.set(m_listener, const_cast<JSLazyEventListener*>(this));
    }
}

} // namespace WebCore
