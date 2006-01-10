// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef _KJS_BINDING_H_
#define _KJS_BINDING_H_

#include <kjs/interpreter.h>
#include <qvariant.h>
#include <kjs/lookup.h>

#if __APPLE__
#include <JavaScriptCore/runtime.h>
#endif

class Frame;

namespace DOM {
    class DocumentImpl;
    class EventImpl;
    class NodeImpl;
}

namespace KJS {

  /**
   * Base class for all objects in this binding.
   */
  class DOMObject : public JSObject {
  protected:
    DOMObject() : JSObject() {}
  public:
    virtual UString toString(ExecState *exec) const;
  };

  /**
   * Base class for all functions in this binding.
   */
  class DOMFunction : public JSObject {
  protected:
    DOMFunction() : JSObject() {}
  public:
    virtual bool implementsCall() const { return true; }
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual JSValue *toPrimitive(ExecState *exec, Type) const { return jsString(toString(exec)); }
    virtual UString toString(ExecState *) const { return UString("[function]"); }
  };

  class DOMNode;

  /**
   * We inherit from Interpreter, to save a pointer to the HTML part
   * that the interpreter runs for.
   * The interpreter also stores the DOM object - >KJS::DOMObject cache.
   */
  class ScriptInterpreter : public Interpreter
  {
  public:
    ScriptInterpreter(JSObject *global, Frame *frame);
    virtual ~ScriptInterpreter();

    static DOMObject* getDOMObject(void* objectHandle);
    static void putDOMObject(void* objectHandle, DOMObject* obj);
    static void forgetDOMObject(void* objectHandle);

    static DOMNode *getDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *node);
    static void putDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *nodeHandle, DOMNode *nodeWrapper);
    static void forgetDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *node);
    static void forgetAllDOMNodesForDocument(DOM::DocumentImpl *document);
    static void updateDOMNodeDocument(DOM::NodeImpl *nodeHandle, DOM::DocumentImpl *oldDoc, DOM::DocumentImpl *newDoc);

    Frame* frame() const { return m_frame; }

    virtual int rtti() { return 1; }

    /**
     * Set the event that is triggering the execution of a script, if any
     */
    void setCurrentEvent( DOM::EventImpl *evt ) { m_evt = evt; }
    void setInlineCode( bool inlineCode ) { m_inlineCode = inlineCode; }
    void setProcessingTimerCallback( bool timerCallback ) { m_timerCallback = timerCallback; }
    /**
     * "Smart" window.open policy
     */
    bool wasRunByUserGesture() const;

    virtual void mark();
    
    DOM::EventImpl *getCurrentEvent() const { return m_evt; }

    virtual bool isGlobalObject(JSValue *v);
    virtual Interpreter *interpreterForGlobalObject (const JSValue *imp);
    virtual bool isSafeScript (const Interpreter *target);
    virtual void *createLanguageInstanceForValue (ExecState *exec, int language, JSObject *value, const Bindings::RootObject *origin, const Bindings::RootObject *current);
    void *createObjcInstanceForValue (ExecState *exec, JSObject *value, const Bindings::RootObject *origin, const Bindings::RootObject *current);

  private:
    Frame* m_frame;

    DOM::EventImpl *m_evt;
    bool m_inlineCode;
    bool m_timerCallback;
  };

  /**
   * Retrieve from cache, or create, a KJS object around a DOM object
   */
  template<class DOMObj, class KJSDOMObj>
  inline JSValue *cacheDOMObject(ExecState *exec, DOMObj *domObj)
  {
    if (!domObj)
      return jsNull();
    ScriptInterpreter *interp = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter());
    if (DOMObject *ret = interp->getDOMObject(domObj))
      return ret;
    DOMObject *ret = new KJSDOMObj(exec, domObj);
    interp->putDOMObject(domObj, ret);
    return ret;
  }

  // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
  void setDOMException(ExecState *exec, int DOMExceptionCode);

  // Helper class to call setDOMException on exit without adding lots of separate calls to that function.
  class DOMExceptionTranslator {
  public:
    explicit DOMExceptionTranslator(ExecState *exec) : m_exec(exec), m_code(0) { }
    ~DOMExceptionTranslator() { setDOMException(m_exec, m_code); }
    operator int &() { return m_code; }
  private:
    ExecState *m_exec;
    int m_code;
  };

  /**
   *  Get a String object, or jsNull() if s is null
   */
  JSValue *jsStringOrNull(const DOM::DOMString&);

  /**
   *  Get a DOMString object or a null DOMString if the value is null
   */
  DOM::DOMString valueToStringWithNullCheck(ExecState* exec, JSValue *val);
  
  /**
   * Convert a KJS value into a QVariant
   */
  QVariant ValueToVariant(ExecState* exec, JSValue *val);

} // namespace

#endif
