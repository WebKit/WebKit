/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "GCController.h"
#include "JSDocument.h"
#include "JSDOMWindow.h"
#include "Page.h"
#include "Settings.h"
#include "kjs_events.h"
#include "kjs_window.h"

#if ENABLE(SVG)
#include "JSSVGLazyEventListener.h"
#endif

using namespace KJS;

namespace WebCore {

KJSProxy::KJSProxy(Frame* frame)
{
    m_frame = frame;
    m_handlerLineno = 0;
}

KJSProxy::~KJSProxy()
{
    // Check for <rdar://problem/4876466>. In theory, no JS should be executing
    // in our interpreter. 
    ASSERT(!m_script || !m_script->context());
    
    if (m_script) {
        m_script = 0;
    
        // It's likely that destroying the interpreter has created a lot of garbage.
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
    bool inlineCode = filename.isNull();

    m_script->setInlineCode(inlineCode);

    JSLock lock;

    // Evaluating the JavaScript could cause the frame to be deallocated
    // so we start the keep alive timer here.
    m_frame->keepAlive();
    
    JSValue* thisNode = Window::retrieve(m_frame);
  
    m_script->startTimeoutCheck();
    Completion comp = m_script->evaluate(filename, baseLine, reinterpret_cast<const KJS::UChar*>(str.characters()), str.length(), thisNode);
    m_script->stopTimeoutCheck();
  
    if (comp.complType() == Normal || comp.complType() == ReturnValue)
        return comp.value();

    if (comp.complType() == Throw) {
        UString errorMessage = comp.value()->toString(m_script->globalExec());
        int lineNumber = comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "line")->toInt32(m_script->globalExec());
        UString sourceURL = comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "sourceURL")->toString(m_script->globalExec());
        if (Page* page = m_frame->page())
            page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, errorMessage, lineNumber, sourceURL);
    }

    return 0;
}

void KJSProxy::clear() {
  // clear resources allocated by the interpreter, and make it ready to be used by another page
  // We have to keep it, so that the Window object for the frame remains the same.
  // (we used to delete and re-create it, previously)
  if (m_script) {
    Window *win = Window::retrieveWindow(m_frame);
    if (win)
        win->clear();
  }
}

EventListener* KJSProxy::createHTMLEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return new JSLazyEventListener(functionName, code, Window::retrieveWindow(m_frame), node, m_handlerLineno);
}

#if ENABLE(SVG)
EventListener* KJSProxy::createSVGEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock;
    return new JSSVGLazyEventListener(functionName, code, Window::retrieveWindow(m_frame), node, m_handlerLineno);
}
#endif

void KJSProxy::finishedWithEvent(Event* event)
{
  // This is called when the DOM implementation has finished with a particular event. This
  // is the case in sitations where an event has been created just for temporary usage,
  // e.g. an image load or mouse move. Once the event has been dispatched, it is forgotten
  // by the DOM implementation and so does not need to be cached still by the interpreter
  m_script->forgetDOMObject(event);
}

ScriptInterpreter* KJSProxy::interpreter()
{
  initScriptIfNeeded();
  ASSERT(m_script);
  return m_script.get();
}

void KJSProxy::initScriptIfNeeded()
{
  if (m_script)
    return;

  // Build the global object - which is a Window instance
  JSLock lock;
  JSObject* globalObject = new JSDOMWindow(m_frame->domWindow());

  // Create a KJS interpreter for this frame
  m_script = new ScriptInterpreter(globalObject, m_frame);

  String userAgent = m_frame->loader()->userAgent(m_frame->document() ? m_frame->document()->URL() : KURL());
  if (userAgent.find("Microsoft") >= 0 || userAgent.find("MSIE") >= 0)
    m_script->setCompatMode(Interpreter::IECompat);
  else
    // If we find "Mozilla" but not "(compatible, ...)" we are a real Netscape
    if (userAgent.find("Mozilla") >= 0 && userAgent.find("compatible") == -1)
      m_script->setCompatMode(Interpreter::NetscapeCompat);

  m_frame->loader()->dispatchWindowObjectAvailable();
}
    
void KJSProxy::clearDocumentWrapper() 
{
    if (!m_script)
        return;

    JSLock lock;
    m_script->globalObject()->removeDirect("document");
}

}
