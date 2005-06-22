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
#include <qptrdict.h>
#include <kjs/lookup.h>
#include <kjs/protect.h>

#if APPLE_CHANGES
#include <JavaScriptCore/runtime.h>
#endif

class KHTMLPart;

namespace DOM {
    class DocumentImpl;
    class EventImpl;
    class NodeImpl;
}

namespace KJS {

  /**
   * Base class for all objects in this binding - get() and put() run
   * tryGet() and tryPut() respectively, and catch exceptions if they
   * occur.
   */
  class DOMObject : public ObjectImp {
  public:
    DOMObject() : ObjectImp() {}
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const
      { return ObjectImp::get(exec, propertyName); }

    virtual void put(ExecState *exec, const Identifier &propertyName,
                     const Value &value, int attr = None);
    virtual void tryPut(ExecState *exec, const Identifier &propertyName,
                        const Value& value, int attr = None)
      { ObjectImp::put(exec,propertyName,value,attr); }

    virtual UString toString(ExecState *exec) const;
  };

  /**
   * Base class for all functions in this binding - get() and call() run
   * tryGet() and tryCall() respectively, and catch exceptions if they
   * occur.
   */
  class DOMFunction : public ObjectImp {
  public:
    DOMFunction() : ObjectImp( /* proto? */ ) {}
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual Value tryGet(ExecState *exec, const Identifier &propertyName) const
      { return ObjectImp::get(exec, propertyName); }

    virtual bool implementsCall() const { return true; }
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    virtual Value tryCall(ExecState *exec, Object &thisObj, const List&args)
      { return ObjectImp::call(exec, thisObj, args); }
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual Value toPrimitive(ExecState *exec, Type) const { return String(toString(exec)); }
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
    ScriptInterpreter( const Object &global, KHTMLPart* part );
    virtual ~ScriptInterpreter();

    static DOMObject* getDOMObject( void* objectHandle ) {
      return domObjects()[objectHandle];
    }
    static void putDOMObject( void* objectHandle, DOMObject* obj ) {
      domObjects().insert( objectHandle, obj );
    }
    static bool deleteDOMObject( void* objectHandle ) {
      return domObjects().remove( objectHandle );
    }

    static void forgetDOMObject( void* objectHandle );


    static DOMNode *getDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *node);
    static void putDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *nodeHandle, DOMNode *nodeWrapper);
    static void forgetDOMNodeForDocument(DOM::DocumentImpl *document, DOM::NodeImpl *node);
    static void forgetAllDOMNodesForDocument(DOM::DocumentImpl *document);
    static void updateDOMNodeDocument(DOM::NodeImpl *nodeHandle, DOM::DocumentImpl *oldDoc, DOM::DocumentImpl *newDoc);



    KHTMLPart* part() const { return m_part; }

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

#if APPLE_CHANGES
    virtual bool isGlobalObject(const Value &v);
    virtual Interpreter *interpreterForGlobalObject (const ValueImp *imp);
    virtual bool isSafeScript (const Interpreter *target);
    virtual void *createLanguageInstanceForValue (ExecState *exec, Bindings::Instance::BindingLanguage language, const Object &value, const Bindings::RootObject *origin, const Bindings::RootObject *current);
    void *createObjcInstanceForValue (ExecState *exec, const Object &value, const Bindings::RootObject *origin, const Bindings::RootObject *current);
#endif

  private:
    KHTMLPart* m_part;

    static QPtrDict<DOMObject> &domObjects();
    static QPtrDict<QPtrDict<DOMNode> > &domNodesPerDocument();

    DOM::EventImpl *m_evt;
    bool m_inlineCode;
    bool m_timerCallback;
  };

  /**
   * Retrieve from cache, or create, a KJS object around a DOM object
   */
  template<class DOMObj, class KJSDOMObj>
  inline ValueImp *cacheDOMObject(ExecState *exec, DOMObj *domObj)
  {
    if (!domObj)
      return null();
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
   *  Get a String object, or Null() if s is null
   */
  Value getStringOrNull(DOM::DOMString s);

  /**
   * Convert a KJS value into a QVariant
   * Deprecated: Use variant instead.
   */
  QVariant ValueToVariant(ExecState* exec, const Value& val);

  /**
   * We need a modified version of lookupGet because
   * we call tryGet instead of get, in DOMObjects.
   */
  template <class FuncImp, class ThisImp, class ParentImp>
  inline Value DOMObjectLookupGet(ExecState *exec, const Identifier &propertyName,
                                  const HashTable* table, const ThisImp* thisObj)
  {
    const HashEntry* entry = Lookup::findEntry(table, propertyName);

    if (!entry) // not found, forward to parent
      return thisObj->ParentImp::tryGet(exec, propertyName);

    if (entry->attr & Function)
      return lookupOrCreateFunction<FuncImp>(exec, propertyName, thisObj, entry->value, entry->params, entry->attr);
    return thisObj->getValueProperty(exec, entry->value);
  }

  /**
   * Simplified version of DOMObjectLookupGet in case there are no
   * functions, only "values".
   */
  template <class ThisImp, class ParentImp>
  inline Value DOMObjectLookupGetValue(ExecState *exec, const Identifier &propertyName,
                                       const HashTable* table, const ThisImp* thisObj)
  {
    const HashEntry* entry = Lookup::findEntry(table, propertyName);

    if (!entry) // not found, forward to parent
      return thisObj->ParentImp::tryGet(exec, propertyName);

    if (entry->attr & Function)
      fprintf(stderr, "Function bit set! Shouldn't happen in lookupValue!\n" );
    return thisObj->getValueProperty(exec, entry->value);
  }

  /**
   * We need a modified version of lookupPut because
   * we call tryPut instead of put, in DOMObjects.
   */
  template <class ThisImp, class ParentImp>
  inline void DOMObjectLookupPut(ExecState *exec, const Identifier &propertyName,
                                 const Value& value, int attr,
                                 const HashTable* table, ThisImp* thisObj)
  {
    const HashEntry* entry = Lookup::findEntry(table, propertyName);

    if (!entry) // not found: forward to parent
      thisObj->ParentImp::tryPut(exec, propertyName, value, attr);
    else if (entry->attr & Function) // function: put as override property
      thisObj->ObjectImp::put(exec, propertyName, value, attr);
    else if (entry->attr & ReadOnly) // readonly! Can't put!
#ifdef KJS_VERBOSE
      fprintf(stderr,"Attempt to change value of readonly property '%s'\n",propertyName.ascii());
#else
    ; // do nothing
#endif
    else
      thisObj->putValue(exec, entry->value, value, attr);
  }

  /**
   * This template method retrieves or create an object that is unique
   * (for a given interpreter) The first time this is called (for a given
   * property name), the Object will be constructed, and set as a property
   * of the interpreter's global object. Later calls will simply retrieve
   * that cached object. Note that the object constructor must take 1 argument, exec.
   */
  template <class ClassCtor>
  inline ObjectImp *cacheGlobalObject(ExecState *exec, const Identifier &propertyName)
  {
    ObjectImp *globalObject = static_cast<ObjectImp *>(exec->lexicalInterpreter()->globalObject().imp());
    ValueImp *obj = globalObject->getDirect(propertyName);
    if (obj) {
      assert(obj->isObject());
      return static_cast<ObjectImp *>(obj);
    }
    ObjectImp *newObject = new ClassCtor(exec);
    globalObject->put(exec, propertyName, Value(newObject), Internal);
    return newObject;
  }

  /**
   * Helpers to define prototype objects (each of which simply implements
   * the functions for a type of objects).
   * Sorry for this not being very readable, but it actually saves much copy-n-paste.
   * ParentProto is not our base class, it's the object we use as fallback.
   * The reason for this is that there should only be ONE DOMNode.hasAttributes (e.g.),
   * not one in each derived class. So we link the (unique) prototypes between them.
   *
   * Using those macros is very simple: define the hashtable (e.g. "DOMNodeProtoTable"), then
   * DEFINE_PROTOTYPE("DOMNode",DOMNodeProto)
   * IMPLEMENT_PROTOFUNC(DOMNodeProtoFunc)
   * IMPLEMENT_PROTOTYPE(DOMNodeProto,DOMNodeProtoFunc)
   * and use DOMNodeProto::self(exec) as prototype in the DOMNode constructor.
   * If the prototype has a "parent prototype", e.g. DOMElementProto falls back on DOMNodeProto,
   * then the last line will use IMPLEMENT_PROTOTYPE_WITH_PARENT, with DOMNodeProto as last argument.
   */
#define DEFINE_PROTOTYPE(ClassName,ClassProto) \
  class ClassProto : public ObjectImp { \
    friend ObjectImp *cacheGlobalObject<ClassProto>(ExecState *exec, const Identifier &propertyName); \
  public: \
    static ObjectImp *self(ExecState *exec) \
    { \
      return cacheGlobalObject<ClassProto>( exec, "[[" ClassName ".prototype]]" ); \
    } \
  protected: \
    ClassProto( ExecState *exec ) \
      : ObjectImp( exec->lexicalInterpreter()->builtinObjectPrototype() ) {} \
    \
  public: \
    virtual const ClassInfo *classInfo() const { return &info; } \
    static const ClassInfo info; \
    Value get(ExecState *exec, const Identifier &propertyName) const; \
    bool hasProperty(ExecState *exec, const Identifier &propertyName) const; \
  }; \
  const ClassInfo ClassProto::info = { ClassName, 0, &ClassProto##Table, 0 };

#define IMPLEMENT_PROTOTYPE(ClassProto,ClassFunc) \
    Value ClassProto::get(ExecState *exec, const Identifier &propertyName) const \
    { \
      /*fprintf( stderr, "%sProto::get(%s) [in macro, no parent]\n", info.className, propertyName.ascii());*/ \
      return lookupGetFunction<ClassFunc,ObjectImp>(exec, propertyName, &ClassProto##Table, this ); \
    } \
    bool ClassProto::hasProperty(ExecState *exec, const Identifier &propertyName) const \
    { /*stupid but we need this to have a common macro for the declaration*/ \
      return ObjectImp::hasProperty(exec, propertyName); \
    }

#define IMPLEMENT_PROTOTYPE_WITH_PARENT(ClassProto,ClassFunc,ParentProto)  \
    Value ClassProto::get(ExecState *exec, const Identifier &propertyName) const \
    { \
      /*fprintf( stderr, "%sProto::get(%s) [in macro]\n", info.className, propertyName.ascii());*/ \
      Value val = lookupGetFunction<ClassFunc,ObjectImp>(exec, propertyName, &ClassProto##Table, this ); \
      if ( val.type() != UndefinedType ) return val; \
      /* Not found -> forward request to "parent" prototype */ \
      return ParentProto::self(exec)->get( exec, propertyName ); \
    } \
    bool ClassProto::hasProperty(ExecState *exec, const Identifier &propertyName) const \
    { \
      if (ObjectImp::hasProperty(exec, propertyName)) \
        return true; \
      return ParentProto::self(exec)->hasProperty(exec, propertyName); \
    }

#define IMPLEMENT_PROTOFUNC(ClassFunc) \
  class ClassFunc : public DOMFunction { \
  public: \
    ClassFunc(ExecState *exec, int i, int len) \
       : DOMFunction( /*proto? */ ), id(i) { \
       Value protect(this); \
       put(exec,lengthPropertyName,Number(len),DontDelete|ReadOnly|DontEnum); \
    } \
    /** You need to implement that one */ \
    virtual Value tryCall(ExecState *exec, Object &thisObj, const List &args); \
  private: \
    int id; \
  };

} // namespace

#endif
