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
#include <khtml_part.h>
#include <kprotocolmanager.h>
#include <kdebug.h>
#include <kjs/collector.h>

using namespace KJS;

using DOM::EventListener;

extern "C" {
  KJSProxy *kjs_html_init(KHTMLPart *khtmlpart);
}

class KJSProxyImpl : public KJSProxy {
public:
  KJSProxyImpl(KHTMLPart *part);
  virtual ~KJSProxyImpl();
  virtual QVariant evaluate(QString filename, int baseLine, const QString&str, DOM::NodeImpl *n);
  virtual void clear();
  virtual DOM::EventListener *createHTMLEventHandler(QString sourceUrl, QString code, DOM::NodeImpl *node);
  virtual void finishedWithEvent(DOM::EventImpl *event);
  virtual KJS::ScriptInterpreter *interpreter();

  virtual void setDebugEnabled(bool enabled);
  virtual bool paused() const;
  virtual void setSourceFile(QString url, QString code);
  virtual void appendSourceFile(QString url, QString code);

  void initScript();

private:
  KJS::ScriptInterpreter* m_script;
  bool m_debugEnabled;
#ifndef NDEBUG
  static int s_count;
#endif
};

#ifndef NDEBUG
int KJSProxyImpl::s_count = 0;
#endif

KJSProxyImpl::KJSProxyImpl(KHTMLPart *part)
{
  m_script = 0;
  m_part = part;
  m_debugEnabled = false;
#ifndef NDEBUG
  s_count++;
#endif
}

KJSProxyImpl::~KJSProxyImpl()
{
  InterpreterLock lock;
  delete m_script;

#ifndef NDEBUG
  s_count--;
  // If it was the last interpreter, we should have nothing left
#ifdef KJS_DEBUG_MEM
  if (s_count == 0)
    Interpreter::finalCheck();
#endif
#endif
}

QVariant KJSProxyImpl::evaluate(QString filename, int baseLine,
                                const QString&str, DOM::NodeImpl *n) 
{
  // evaluate code. Returns the JS return value or an invalid QVariant
  // if there was none, an error occured or the type couldn't be converted.

  initScript();
  // inlineCode is true for <a href="javascript:doSomething()">
  // and false for <script>doSomething()</script>. Check if it has the
  // expected value in all cases.
  // See smart window.open policy for where this is used.
  bool inlineCode = filename.isNull();

#ifdef KJS_DEBUGGER
  if (inlineCode)
    filename = "(unknown file)";
  if (KJSDebugWin::instance())
    KJSDebugWin::instance()->setNextSourceInfo(filename,baseLine);
#endif

  m_script->setInlineCode(inlineCode);

  InterpreterLock lock;

  KJS::ValueImp *thisNode = n ? Window::retrieve(m_part) : getDOMNode(m_script->globalExec(), n);
  UString code(str);
  Completion comp = m_script->evaluate(filename, baseLine, code, thisNode);

  bool success = ( comp.complType() == Normal ) || ( comp.complType() == ReturnValue );  

  // let's try to convert the return value
  if (success && comp.value())
    return ValueToVariant(m_script->globalExec(), comp.value());

  if ( comp.complType() == Throw ) {
    UString errorMessage = comp.value()->toString(m_script->globalExec());
    int lineNumber =  comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "line")->toInt32(m_script->globalExec());
    UString sourceURL = comp.value()->toObject(m_script->globalExec())->get(m_script->globalExec(), "sourceURL")->toString(m_script->globalExec());

#if APPLE_CHANGES
    KWQ(m_part)->addMessageToConsole(errorMessage.qstring(), lineNumber, sourceURL.qstring());
#else
    kdWarning(6070) << "Script threw exception: " << errorMessage.qstring() << endl;
#endif
  }
  return QVariant();
}

void KJSProxyImpl::clear() {
  // clear resources allocated by the interpreter, and make it ready to be used by another page
  // We have to keep it, so that the Window object for the part remains the same.
  // (we used to delete and re-create it, previously)
  if (m_script) {
#ifdef KJS_DEBUGGER
    KJSDebugWin *debugWin = KJSDebugWin::instance();
    if (debugWin && debugWin->currentScript() == m_script) {
        debugWin->setMode(KJSDebugWin::Stop);
    }
#endif
    Window *win = Window::retrieveWindow(m_part);
    if (win)
        win->clear( m_script->globalExec() );
  }
}

DOM::EventListener *KJSProxyImpl::createHTMLEventHandler(QString sourceUrl, QString code, DOM::NodeImpl *node)
{
#ifdef KJS_DEBUGGER
  if (KJSDebugWin::instance())
    KJSDebugWin::instance()->setNextSourceInfo(sourceUrl,m_handlerLineno);
#else
  Q_UNUSED(sourceUrl);
#endif

  initScript();
  InterpreterLock lock;
  return KJS::Window::retrieveWindow(m_part)->getJSLazyEventListener(code,node,m_handlerLineno);
}

void KJSProxyImpl::finishedWithEvent(DOM::EventImpl *event)
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
  m_part->keepAlive();
  return m_script;
}

void KJSProxyImpl::setDebugEnabled(bool enabled)
{
#ifdef KJS_DEBUGGER
  m_debugEnabled = enabled;
  if (m_script)
      m_script->setDebuggingEnabled(enabled);
  // NOTE: this is consistent across all KJSProxyImpl instances, as we only
  // ever have 1 debug window
  if (!enabled && KJSDebugWin::instance()) {
    KJSDebugWin::destroyInstance();
  }
  else if (enabled && !KJSDebugWin::instance()) {
    KJSDebugWin::createInstance();
    initScript();
    KJSDebugWin::instance()->attach(m_script);
  }
#else
  Q_UNUSED(enabled);
#endif
}

bool KJSProxyImpl::paused() const
{
#ifdef KJS_DEBUGGER
  if (KJSDebugWin::instance())
    return KJSDebugWin::instance()->inSession();
#endif
  return false;
}

void KJSProxyImpl::setSourceFile(QString url, QString code)
{
#ifdef KJS_DEBUGGER
  if (KJSDebugWin::instance())
    KJSDebugWin::instance()->setSourceFile(url,code);
#else
  Q_UNUSED(url);
  Q_UNUSED(code);
#endif

}

void KJSProxyImpl::appendSourceFile(QString url, QString code)
{
#ifdef KJS_DEBUGGER
  if (KJSDebugWin::instance())
    KJSDebugWin::instance()->appendSourceFile(url,code);
#else
  Q_UNUSED(url);
  Q_UNUSED(code);
#endif
}

// Implementation of the debug() function
class TestFunctionImp : public ObjectImp {
public:
  TestFunctionImp() : ObjectImp() {}
  virtual bool implementsCall() const { return true; }
  virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
};

ValueImp *TestFunctionImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  fprintf(stderr,"--> %s\n",args[0]->toString(exec).ascii());
  return Undefined();
}

void KJSProxyImpl::initScript()
{
  if (m_script)
    return;

  // Build the global object - which is a Window instance
  KJS::InterpreterLock lock;
  ObjectImp *globalObject( new Window(m_part) );

  // Create a KJS interpreter for this part
  m_script = new KJS::ScriptInterpreter(globalObject, m_part);

#ifdef KJS_DEBUGGER
  m_script->setDebuggingEnabled(m_debugEnabled);
#endif
  //m_script->enableDebug();
  globalObject->put(m_script->globalExec(), "debug", new TestFunctionImp(), Internal);

#if APPLE_CHANGES
  QString userAgent = KWQ(m_part)->userAgent();
#else
  QString userAgent = KProtocolManager::userAgentForHost(m_part->url().host());
#endif
  if (userAgent.find(QString::fromLatin1("Microsoft")) >= 0 ||
      userAgent.find(QString::fromLatin1("MSIE")) >= 0)
    m_script->setCompatMode(Interpreter::IECompat);
  else
    // If we find "Mozilla" but not "(compatible, ...)" we are a real Netscape
    if (userAgent.find(QString::fromLatin1("Mozilla")) >= 0 &&
        userAgent.find(QString::fromLatin1("compatible")) == -1)
      m_script->setCompatMode(Interpreter::NetscapeCompat);
}

// Helper method, so that all classes which need jScript() don't need to be added
// as friend to KHTMLPart
KJSProxy * KJSProxy::proxy( KHTMLPart *part )
{
    return part ? part->jScript() : NULL;
}

// initialize HTML module
KJSProxy *kjs_html_init(KHTMLPart *khtmlpart)
{
  return new KJSProxyImpl(khtmlpart);
}
