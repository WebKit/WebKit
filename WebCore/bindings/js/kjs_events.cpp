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
#include "kjs_events.h"

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
#include "JSEventTargetNode.h"
#include "kjs_proxy.h"
#include <kjs/function_object.h>

using namespace KJS;

namespace WebCore {

using namespace EventNames;

JSAbstractEventListener::JSAbstractEventListener(bool html)
    : m_html(html)
{
}

void JSAbstractEventListener::handleEvent(Event* ele, bool isWindowEvent)
{
    Event* event = ele;

    JSObject* listener = listenerObj();
    if (!listener)
        return;

    JSDOMWindow* window = this->window();
    // Null check as clearWindow() can clear this and we still get called back by
    // xmlhttprequest objects. See http://bugs.webkit.org/show_bug.cgi?id=13275
    if (!window)
        return;
    Frame* frame = window->impl()->frame();
    if (!frame)
        return;
    KJSProxy* scriptProxy = frame->scriptProxy();
    if (!scriptProxy->isEnabled() || scriptProxy->isPaused())
        return;

    JSLock lock;

    ExecState* exec = window->globalExec();

    JSValue* handleEventFuncValue = listener->get(exec, "handleEvent");
    JSObject* handleEventFunc = 0;
    if (handleEventFuncValue->isObject()) {
        handleEventFunc = static_cast<JSObject*>(handleEventFuncValue);
        if (!handleEventFunc->implementsCall())
            handleEventFunc = 0;
    }

    if (handleEventFunc || listener->implementsCall()) {
        ref();

        List args;
        args.append(toJS(exec, event));

        Event* savedEvent = window->currentEvent();
        window->setCurrentEvent(event);

        JSValue* retval;
        if (handleEventFunc) {
            window->startTimeoutCheck();
            retval = handleEventFunc->call(exec, listener, args);
        } else {
            JSObject* thisObj;
            if (isWindowEvent)
                thisObj = window->shell();
            else
                thisObj = static_cast<JSObject*>(toJS(exec, event->currentTarget()));
            window->startTimeoutCheck();
            retval = listener->call(exec, thisObj, args);
        }
        window->stopTimeoutCheck();

        window->setCurrentEvent(savedEvent);

        if (exec->hadException()) {
            JSObject* exception = exec->exception()->toObject(exec);
            String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
            int lineNumber = exception->get(exec, "line")->toInt32(exec);
            String sourceURL = exception->get(exec, "sourceURL")->toString(exec);
            if (Interpreter::shouldPrintExceptions())
                printf("(event handler):%s\n", message.utf8().data());
            frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, sourceURL);
            exec->clearException();
        } else {
            if (!retval->isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval->toString(exec));
            if (m_html) {
                bool retvalbool;
                if (retval->getBoolean(retvalbool) && !retvalbool)
                    event->preventDefault();
            }
        }

        Document::updateDocumentsRendering();
        deref();
    }
}

bool JSAbstractEventListener::isHTMLEventListener() const
{
    return m_html;
}

// -------------------------------------------------------------------------

JSUnprotectedEventListener::JSUnprotectedEventListener(JSObject* listener, JSDOMWindow* window, bool html)
    : JSAbstractEventListener(html)
    , m_listener(listener)
    , m_window(window)
{
    if (m_listener) {
        JSDOMWindow::UnprotectedListenersMap& listeners = html
            ? window->jsUnprotectedHTMLEventListeners() : window->jsUnprotectedEventListeners();
        listeners.set(m_listener, this);
    }
}

JSUnprotectedEventListener::~JSUnprotectedEventListener()
{
    if (m_listener && m_window) {
        JSDOMWindow::UnprotectedListenersMap& listeners = isHTMLEventListener()
            ? m_window->jsUnprotectedHTMLEventListeners() : m_window->jsUnprotectedEventListeners();
        listeners.remove(m_listener);
    }
}

JSObject* JSUnprotectedEventListener::listenerObj() const
{
    return m_listener;
}

JSDOMWindow* JSUnprotectedEventListener::window() const
{
    return m_window;
}

void JSUnprotectedEventListener::clearWindow()
{
    m_window = 0;
}

void JSUnprotectedEventListener::mark()
{
    if (m_listener && !m_listener->marked())
        m_listener->mark();
}

#ifndef NDEBUG
#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif
WTFLogChannel LogWebCoreEventListenerLeaks = { 0x00000000, "", WTFLogChannelOn };

struct EventListenerCounter {
    static unsigned count;
    ~EventListenerCounter()
    {
        if (count)
            LOG(WebCoreEventListenerLeaks, "LEAK: %u EventListeners\n", count);
    }
};
unsigned EventListenerCounter::count = 0;
static EventListenerCounter eventListenerCounter;
#endif

// -------------------------------------------------------------------------

JSEventListener::JSEventListener(JSObject* listener, JSDOMWindow* window, bool html)
    : JSAbstractEventListener(html)
    , m_listener(listener)
    , m_window(window)
{
    if (m_listener) {
        JSDOMWindow::ListenersMap& listeners = html
            ? m_window->jsHTMLEventListeners() : m_window->jsEventListeners();
        listeners.set(m_listener, this);
    }
#ifndef NDEBUG
    ++eventListenerCounter.count;
#endif
}

JSEventListener::~JSEventListener()
{
    if (m_listener && m_window) {
        JSDOMWindow::ListenersMap& listeners = isHTMLEventListener()
            ? m_window->jsHTMLEventListeners() : m_window->jsEventListeners();
        listeners.remove(m_listener);
    }
#ifndef NDEBUG
    --eventListenerCounter.count;
#endif
}

JSObject* JSEventListener::listenerObj() const
{
    return m_listener;
}

JSDOMWindow* JSEventListener::window() const
{
    return m_window;
}

void JSEventListener::clearWindow()
{
    m_window = 0;
}

// -------------------------------------------------------------------------

JSLazyEventListener::JSLazyEventListener(const String& functionName, const String& code, JSDOMWindow* window, Node* node, int lineNumber)
    : JSEventListener(0, window, true)
    , m_functionName(functionName)
    , m_code(code)
    , m_parsed(false)
    , m_lineNumber(lineNumber)
    , m_originalNode(node)
{
    // We don't retain the original node because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, parseCode will be called and
    // then originalNode is no longer needed.
}

JSObject* JSLazyEventListener::listenerObj() const
{
    parseCode();
    return m_listener;
}

JSValue* JSLazyEventListener::eventParameterName() const
{
    static ProtectedPtr<JSValue> eventString = jsString("event");
    return eventString.get();
}

void JSLazyEventListener::parseCode() const
{
    if (m_parsed)
        return;
    m_parsed = true;

    Frame* frame = window()->impl()->frame();
    if (frame && frame->scriptProxy()->isEnabled()) {
        ExecState* exec = window()->globalExec();

        JSLock lock;
        JSObject* constr = window()->functionConstructor();
        List args;

        UString sourceURL(frame->loader()->url().string());
        args.append(eventParameterName());
        args.append(jsString(m_code));
        m_listener = constr->construct(exec, args, m_functionName, sourceURL, m_lineNumber); // FIXME: is globalExec ok ?

        FunctionImp* listenerAsFunction = static_cast<FunctionImp*>(m_listener.get());

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
                static_cast<JSEventTargetNode*>(thisObj)->pushEventHandlerScope(exec, scope);
                listenerAsFunction->setScope(scope);
            }
        }
    }

    // no more need to keep the unparsed code around
    m_functionName = String();
    m_code = String();

    if (m_listener) {
        JSDOMWindow::ListenersMap& listeners = isHTMLEventListener()
            ? window()->jsHTMLEventListeners() : window()->jsEventListeners();
        listeners.set(m_listener, const_cast<JSLazyEventListener*>(this));
    }
}

JSValue* getNodeEventListener(EventTargetNode* n, const AtomicString& eventType)
{
    if (JSAbstractEventListener* listener = static_cast<JSAbstractEventListener*>(n->getHTMLEventListener(eventType))) {
        if (JSValue* obj = listener->listenerObj())
            return obj;
    }
    return jsNull();
}

} // namespace WebCore
