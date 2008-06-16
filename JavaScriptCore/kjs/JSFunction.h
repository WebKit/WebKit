// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_FUNCTION_H
#define KJS_FUNCTION_H

#include "JSVariableObject.h"
#include "SymbolTable.h"
#include "nodes.h"
#include "JSObject.h"

namespace KJS {

  class FunctionBodyNode;
  class FunctionPrototype;
  class JSActivation;
  class JSGlobalObject;

  class InternalFunction : public JSObject {
  public:
    InternalFunction();
    InternalFunction(FunctionPrototype*, const Identifier&);

    virtual CallType getCallData(CallData&);

    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObjec, const List& args) = 0;
    virtual bool implementsHasInstance() const;

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    const Identifier& functionName() const { return m_name; }

  private:
    Identifier m_name;
  };

  class JSFunction : public InternalFunction {
  public:
    JSFunction(ExecState*, const Identifier&, FunctionBodyNode*, ScopeChainNode*);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    virtual ConstructType getConstructData(ConstructData&);
    virtual JSObject* construct(ExecState*, const List& args);

    virtual CallType getCallData(CallData&);
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List& args);

    // Note: unlike body->paramName, this returns Identifier::null for parameters 
    // that will never get set, due to later param having the same name
    Identifier getParameterName(int index);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    RefPtr<FunctionBodyNode> body;

    void setScope(const ScopeChain& s) { _scope = s; }
    ScopeChain& scope() { return _scope; }

    virtual void mark();

  private:
    ScopeChain _scope;

    static JSValue* argumentsGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* callerGetter(ExecState*, const Identifier&, const PropertySlot&);
    static JSValue* lengthGetter(ExecState*, const Identifier&, const PropertySlot&);
  };

  class IndexToNameMap {
  public:
    IndexToNameMap(JSFunction*, const List& args);
    ~IndexToNameMap();
    
    Identifier& operator[](const Identifier& index);
    bool isMapped(const Identifier& index) const;
    void unMap(const Identifier& index);
    
  private:
    unsigned size;
    Identifier* _map;
  };
  
  class Arguments : public JSObject {
  public:
    Arguments(ExecState*, JSFunction* func, const List& args, JSActivation* act);
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static JSValue* mappedIndexGetter(ExecState*, const Identifier&, const PropertySlot& slot);

    JSActivation* _activationObject;
    mutable IndexToNameMap indexToNameMap;
  };

  class PrototypeFunction : public InternalFunction {
  public:
    typedef JSValue* (*JSMemberFunction)(ExecState*, JSObject* thisObj, const List&);

    PrototypeFunction(ExecState*, int len, const Identifier&, JSMemberFunction);
    PrototypeFunction(ExecState*, FunctionPrototype*, int len, const Identifier&, JSMemberFunction);

    virtual JSValue* callAsFunction(ExecState* exec, JSObject* thisObj, const List&);

  private:
    const JSMemberFunction m_function;
  };


  // Just like PrototypeFunction, but callbacks also get passed the JS function object.
  class PrototypeReflexiveFunction : public InternalFunction {
  public:
    typedef JSValue* (*JSMemberFunction)(ExecState*, PrototypeReflexiveFunction*, JSObject* thisObj, const List&);

    PrototypeReflexiveFunction(ExecState*, FunctionPrototype*, int len, const Identifier&, JSMemberFunction, JSGlobalObject* expectedThisObject);

    virtual void mark();
    virtual JSValue* callAsFunction(ExecState* exec, JSObject* thisObj, const List&);

    JSGlobalObject* cachedGlobalObject() const { return m_cachedGlobalObject; }

  private:
    const JSMemberFunction m_function;
    JSGlobalObject* m_cachedGlobalObject;
  };

    // Global Functions
    JSValue* globalFuncEval(ExecState*, PrototypeReflexiveFunction*, JSObject*, const List&);
    JSValue* globalFuncParseInt(ExecState*, JSObject*, const List&);
    JSValue* globalFuncParseFloat(ExecState*, JSObject*, const List&);
    JSValue* globalFuncIsNaN(ExecState*, JSObject*, const List&);
    JSValue* globalFuncIsFinite(ExecState*, JSObject*, const List&);
    JSValue* globalFuncDecodeURI(ExecState*, JSObject*, const List&);
    JSValue* globalFuncDecodeURIComponent(ExecState*, JSObject*, const List&);
    JSValue* globalFuncEncodeURI(ExecState*, JSObject*, const List&);
    JSValue* globalFuncEncodeURIComponent(ExecState*, JSObject*, const List&);
    JSValue* globalFuncEscape(ExecState*, JSObject*, const List&);
    JSValue* globalFuncUnescape(ExecState*, JSObject*, const List&);
#ifndef NDEBUG
    JSValue* globalFuncKJSPrint(ExecState*, JSObject*, const List&);
#endif

    static const double mantissaOverflowLowerBound = 9007199254740992.0;
    double parseIntOverflow(const char*, int length, int radix);

} // namespace

#endif
