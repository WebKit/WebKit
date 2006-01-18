/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "kjs_proxy.h"

#include "kjs_window.h"
#include "kjs_events.h"
#include "NodeImpl.h"
#include "Frame.h"
#include <kjs/collector.h>

using namespace DOM;
using namespace KJS;

KJSProxyImpl::KJSProxyImpl(Frame *frame)
{
    m_script = 0;
    m_frame = frame;
    m_handlerLineno = 0;
}

KJSProxyImpl::~KJSProxyImpl()
{
    JSLock lock;
    delete m_script;
}

QVariant KJSProxyImpl::evaluate(const DOMString& filename, int baseLine, const DOMString& str, NodeImpl *n) 
{
  // evaluate code. Returns the JS return value or an invalid QVariant
  // if there was none, an error occured or the type couldn't be converted.

  initScript();
  // inlineCode is true for <a href="javascript:doSomething()">
  // and false for <script>doSomething()</script>. Check if it has the
  // expected value in all cases.
  // See smart window.open policy for where this is used.
  bool inlineCode = filename.isNull();

  m_script->setInlineCode(inlineCode);

  JSLock lock;

  KJS::JSValue* thisNode = n ? Window::retrieve(m_frame) : getDOMNode(m_script->globalExec(), n);
  Completion comp = m_script->evaluate(filename, baseLine, reinterpret_cast<KJS::UChar *>(str.unicode()), str.length(), thisNode);

  bool success = ( comp.complType() == Normal ) || ( comp.complType() == ReturnValue );  

  // let's try to convert the return value
  if (success && comp.value())
    return ValueToVariant(m_script->globalExec(), comp.value());

  if ( comp.complType() == Throw ) {
    UString errorMessage = comp.value()->toString(m_script->globalExec());
    int lineNumber =  comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "line")->toInt32(m_script->globalExec());
    UString sourceURL = comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "sourceURL")->toString(m_script->globalExec());

    m_frame->addMessageToConsole(errorMessage.domString(), lineNumber, sourceURL.domString());
  }
  return QVariant();
}

void KJSProxyImpl::clear() {
  // clear resources allocated by the interpreter, and make it ready to be used by another page
  // We have to keep it, so that the Window object for the frame remains the same.
  // (we used to delete and re-create it, previously)
  if (m_script) {
    Window *win = Window::retrieveWindow(m_frame);
    if (win)
        win->clear( m_script->globalExec() );
  }
}

EventListener *KJSProxyImpl::createHTMLEventHandler(const DOMString& code, NodeImpl *node)
{
    initScript();
    JSLock lock;
    return KJS::Window::retrieveWindow(m_frame)->getJSLazyEventListener(code, node, m_handlerLineno);
}

void KJSProxyImpl::finishedWithEvent(EventImpl *event)
{
  // This is called when the DOM implementation has finished with a particular event. This
  // is the case in sitations where an event has been created just for temporary usage,
  // e.g. an image load or mouse move. Once the event has been dispatched, it is forgotten
  // by the DOM implementation and so does not need to be cached still by the interpreter
  m_script->forgetDOMObject(event);
}

KJS::ScriptInterpreter *KJSProxyImpl::interpreter()
{
  if (!m_script)
    initScript();
  m_frame->keepAlive();
  return m_script;
}

// Implementation of the debug() function
class TestFunctionImp : public JSObject {
public:
  TestFunctionImp() : JSObject() {}
  virtual bool implementsCall() const { return true; }
  virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
};

JSValue *TestFunctionImp::callAsFunction(ExecState *exec, JSObject */*thisObj*/, const List &args)
{
  fprintf(stderr,"--> %s\n",args[0]->toString(exec).ascii());
  return jsUndefined();
}

void KJSProxyImpl::initScript()
{
  if (m_script)
    return;

  // Build the global object - which is a Window instance
  KJS::JSLock lock;
  JSObject *globalObject( new Window(m_frame) );

  // Create a KJS interpreter for this frame
  m_script = new KJS::ScriptInterpreter(globalObject, m_frame);
  globalObject->put(m_script->globalExec(), "debug", new TestFunctionImp(), Internal);

  QString userAgent = m_frame->userAgent();
  if (userAgent.find(QString::fromLatin1("Microsoft")) >= 0 ||
      userAgent.find(QString::fromLatin1("MSIE")) >= 0)
    m_script->setCompatMode(Interpreter::IECompat);
  else
    // If we find "Mozilla" but not "(compatible, ...)" we are a real Netscape
    if (userAgent.find(QString::fromLatin1("Mozilla")) >= 0 &&
        userAgent.find(QString::fromLatin1("compatible")) == -1)
      m_script->setCompatMode(Interpreter::NetscapeCompat);
}
