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

JSEventListener::JSEventListener(JSObject* function, JSDOMGlobalObject* globalObject, bool isAttribute)
    : m_jsFunction(function)
    , m_globalObject(globalObject)
    , m_isAttribute(isAttribute)
{
    if (!m_isAttribute && m_jsFunction)
        globalObject->jsEventListeners().set(m_jsFunction, this);
}

JSEventListener::~JSEventListener()
{
    if (!m_isAttribute && m_jsFunction && m_globalObject)
        m_globalObject->jsEventListeners().remove(m_jsFunction);
}

JSObject* JSEventListener::jsFunction() const
{
    return m_jsFunction;
}

void JSEventListener::markJSFunction(MarkStack& markStack)
{
    if (m_jsFunction)
        markStack.append(m_jsFunction);
    if (m_globalObject)
        markStack.append(m_globalObject);
}

void JSEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    JSLock lock(SilenceAssertionsOnly);

    JSObject* jsFunction = this->jsFunction();
    if (!jsFunction)
        return;

    JSDOMGlobalObject* globalObject = m_globalObject;
    // Null check as clearGlobalObject() can clear this and we still get called back by
    // xmlhttprequest objects. See http://bugs.webkit.org/show_bug.cgi?id=13275
    // FIXME: Is this check still necessary? Requests are supposed to be stopped before clearGlobalObject() is called.
    ASSERT(globalObject);
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

    JSValue handleEventFunction = jsFunction->get(exec, Identifier(exec, "handleEvent"));
    CallData callData;
    CallType callType = handleEventFunction.getCallData(callData);
    if (callType == CallTypeNone) {
        handleEventFunction = JSValue();
        callType = jsFunction->getCallData(callData);
    }

    if (callType != CallTypeNone) {
        ref();

        MarkedArgumentBuffer args;
        args.append(toJS(exec, globalObject, event));

        Event* savedEvent = globalObject->currentEvent();
        globalObject->setCurrentEvent(event);

        // If this event handler is the first JavaScript to execute, then the
        // dynamic global object should be set to the global object of the
        // window in which the event occurred.
        JSGlobalData* globalData = globalObject->globalData();
        DynamicGlobalObjectScope globalObjectScope(exec, globalData->dynamicGlobalObject ? globalData->dynamicGlobalObject : globalObject);

        JSValue retval;
        if (handleEventFunction) {
            globalObject->globalData()->timeoutChecker.start();
            retval = call(exec, handleEventFunction, callType, callData, jsFunction, args);
        } else {
            JSValue thisValue;
            if (isWindowEvent)
                thisValue = globalObject->toThisObject(exec);
            else
                thisValue = toJS(exec, globalObject, event->currentTarget());
            globalObject->globalData()->timeoutChecker.start();
            retval = call(exec, jsFunction, callType, callData, thisValue, args);
        }
        globalObject->globalData()->timeoutChecker.stop();

        globalObject->setCurrentEvent(savedEvent);

        if (exec->hadException())
            reportCurrentException(exec);
        else {
            if (!retval.isUndefinedOrNull() && event->storesResultAsString())
                event->storeResult(retval.toString(exec));
            if (m_isAttribute) {
                bool retvalbool;
                if (retval.getBoolean(retvalbool) && !retvalbool)
                    event->preventDefault();
            }
        }

        if (scriptExecutionContext->isDocument())
            Document::updateStyleForAllDocuments();
        deref();
    }
}

bool JSEventListener::reportError(const String& message, const String& url, int lineNumber)
{
    JSLock lock(SilenceAssertionsOnly);

    JSObject* jsFunction = this->jsFunction();
    if (!jsFunction)
        return false;

    JSDOMGlobalObject* globalObject = m_globalObject;
    if (!globalObject)
        return false;

    ExecState* exec = globalObject->globalExec();

    CallData callData;
    CallType callType = jsFunction->getCallData(callData);

    if (callType == CallTypeNone)
        return false;

    MarkedArgumentBuffer args;
    args.append(jsString(exec, message));
    args.append(jsString(exec, url));
    args.append(jsNumber(exec, lineNumber));

    // If this event handler is the first JavaScript to execute, then the
    // dynamic global object should be set to the global object of the
    // window in which the event occurred.
    JSGlobalData* globalData = globalObject->globalData();
    DynamicGlobalObjectScope globalObjectScope(exec, globalData->dynamicGlobalObject ? globalData->dynamicGlobalObject : globalObject);    

    JSValue thisValue = globalObject->toThisObject(exec);

    globalObject->globalData()->timeoutChecker.start();
    JSValue returnValue = call(exec, jsFunction, callType, callData, thisValue, args);
    globalObject->globalData()->timeoutChecker.stop();

    // If an error occurs while handling the script error, it should be bubbled up.
    if (exec->hadException()) {
        exec->clearException();
        return false;
    }
    
    bool bubbleEvent;
    return returnValue.getBoolean(bubbleEvent) && !bubbleEvent;
}

bool JSEventListener::virtualisAttribute() const
{
    return m_isAttribute;
}

} // namespace WebCore
