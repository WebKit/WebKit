/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "kjs_proxy.h"

#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "GCController.h"
#include "JSDocument.h"
#include "JSDOMWindow.h"
#include "Page.h"
#include "PageGroup.h"
#include "Settings.h"
#include "StringSourceProvider.h"
#include "kjs_events.h"
#include <kjs/debugger.h>

#if ENABLE(SVG)
#include "JSSVGLazyEventListener.h"
#endif

using namespace KJS;
using namespace WebCore::EventNames;

namespace WebCore {

KJSProxy::KJSProxy(Frame* frame)
    : m_frame(frame)
    , m_handlerLineno(0)
    , m_processingTimerCallback(0)
    , m_processingInlineCode(0)
    , m_paused(false)
{
}

KJSProxy::~KJSProxy()
{
    if (m_windowShell) {
        m_windowShell = 0;
    
        // It's likely that releasing the global object has created a lot of garbage.
        gcController().garbageCollectSoon();
    }
}

JSValue* KJSProxy::evaluate(const String& filename, int baseLine, const String& str) 
{
    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occured or the type couldn't be converted.

    initScriptIfNeeded();
    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    ExecState* exec = m_windowShell->window()->globalExec();
    m_processingInlineCode = filename.isNull();

    JSLock lock;

    // Evaluating the JavaScript could cause the frame to be deallocated
    // so we start the keep alive timer here.
    m_frame->keepAlive();

    m_windowShell->window()->startTimeoutCheck();
    Completion comp = Interpreter::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), filename, baseLine, StringSourceProvider::create(str), m_windowShell);
    m_windowShell->window()->stopTimeoutCheck();

    if (comp.complType() == Normal || comp.complType() == ReturnValue) {
        m_processingInlineCode = false;
        return comp.value();
    }

    if (comp.complType() == Throw) {
        UString errorMessage = comp.value()->toString(exec);
        int lineNumber = comp.value()->toObject(exec)->get(exec, "line")->toInt32(exec);
        UString sourceURL = comp.value()->toObject(exec)->get(exec, "sourceURL")->toString(exec);
        m_frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, errorMessage, lineNumber, sourceURL);
    }

    m_processingInlineCode = false;
    return 0;
}

void KJSProxy::clear()
{
    if (!m_windowShell)
        return;

    JSLock lock;
    m_windowShell->window()->clear();
    m_windowShell->setWindow(new JSDOMWindow(m_frame->domWindow(), m_windowShell));
    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setPageGroupIdentifier(page->group().identifier());
    }

    // There is likely to be a lot of garbage now.
    gcController().garbageCollectSoon();
}

EventListener* KJSProxy::createHTMLEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return new JSLazyEventListener(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}

#if ENABLE(SVG)
EventListener* KJSProxy::createSVGEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return new JSSVGLazyEventListener(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}
#endif

void KJSProxy::finishedWithEvent(Event* event)
{
  // This is called when the DOM implementation has finished with a particular event. This
  // is the case in sitations where an event has been created just for temporary usage,
  // e.g. an image load or mouse move. Once the event has been dispatched, it is forgotten
  // by the DOM implementation and so does not need to be cached still by the interpreter
  ScriptInterpreter::forgetDOMObject(event);
}

void KJSProxy::initScript()
{
    if (m_windowShell)
        return;

    JSLock lock;

    m_windowShell = new JSDOMWindowShell(m_frame->domWindow());
    m_windowShell->updateDocument();

    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setPageGroupIdentifier(page->group().identifier());
    }

    m_frame->loader()->dispatchWindowObjectAvailable();
}

bool KJSProxy::processingUserGesture() const
{
    if (!m_windowShell)
        return false;

    if (Event* event = m_windowShell->window()->currentEvent()) {
        const AtomicString& type = event->type();
        if ( // mouse events
            type == clickEvent || type == mousedownEvent ||
            type == mouseupEvent || type == dblclickEvent ||
            // keyboard events
            type == keydownEvent || type == keypressEvent ||
            type == keyupEvent ||
            // other accepted events
            type == selectEvent || type == changeEvent ||
            type == focusEvent || type == blurEvent ||
            type == submitEvent)
            return true;
    } else { // no event
        if (m_processingInlineCode && !m_processingTimerCallback) {
            // This is the <a href="javascript:window.open('...')> case -> we let it through
            return true;
        }
        // This is the <script>window.open(...)</script> case or a timer callback -> block it
    }
    return false;
}

bool KJSProxy::isEnabled()
{
    Settings* settings = m_frame->settings();
    return (settings && settings->isJavaScriptEnabled());
}

void KJSProxy::attachDebugger(KJS::Debugger* debugger)
{
    if (!m_windowShell)
        return;

    if (debugger)
        debugger->attach(m_windowShell->window());
    else if (KJS::Debugger* currentDebugger = m_windowShell->window()->debugger())
        currentDebugger->detach(m_windowShell->window());
}

} // namespace WebCore
