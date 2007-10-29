// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#include "SymbolTable.h"
#include "object.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace KJS {

  class ActivationImp;
  class FunctionBodyNode;
  class FunctionPrototype;

  class InternalFunctionImp : public JSObject {
  public:
    InternalFunctionImp();
    InternalFunctionImp(FunctionPrototype*);
    InternalFunctionImp(FunctionPrototype*, const Identifier&);

    virtual bool implementsCall() const;
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObjec, const List& args) = 0;
    virtual bool implementsHasInstance() const;

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    const Identifier& functionName() const { return m_name; }

  private:
    Identifier m_name;
  };

  /**
   * @internal
   *
   * The initial value of Function.prototype (and thus all objects created
   * with the Function constructor)
   */
  class FunctionPrototype : public InternalFunctionImp {
  public:
    FunctionPrototype(ExecState *exec);
    virtual ~FunctionPrototype();

    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

  class FunctionImp : public InternalFunctionImp {
    friend class ActivationImp;
  public:
    FunctionImp(ExecState*, const Identifier& name, FunctionBodyNode*, const ScopeChain&);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    virtual bool implementsConstruct() const { return true; }
    virtual JSObject* construct(ExecState*, const List& args);
    
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List& args);
    Completion execute(ExecState*);

    // Note: unlike body->paramName, this returns Identifier::null for parameters 
    // that will never get set, due to later param having the same name
    Identifier getParameterName(int index);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    RefPtr<FunctionBodyNode> body;

    void setScope(const ScopeChain& s) { _scope = s; }
    const ScopeChain& scope() const { return _scope; }

    virtual void mark();

  private:
    ScopeChain _scope;

    static JSValue* argumentsGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* callerGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
  };

  class IndexToNameMap {
  public:
    IndexToNameMap(FunctionImp* func, const List& args);
    ~IndexToNameMap();
    
    Identifier& operator[](int index);
    Identifier& operator[](const Identifier &indexIdentifier);
    bool isMapped(const Identifier& index) const;
    void unMap(const Identifier& index);
    
  private:
    IndexToNameMap(); // prevent construction w/o parameters
    int size;
    Identifier* _map;
  };
  
  class Arguments : public JSObject {
  public:
    Arguments(ExecState*, FunctionImp* func, const List& args, ActivationImp* act);
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static JSValue* mappedIndexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot);

    ActivationImp* _activationObject;
    mutable IndexToNameMap indexToNameMap;
  };

  class ActivationImp : public JSObject {
  public:
    struct LocalStorageEntry {
        LocalStorageEntry()
        {
        }

        LocalStorageEntry(JSValue* v, int a)
            : value(v)
            , attributes(a)
        {
        }

        JSValue* value;
        int attributes;
    };

    typedef Vector<LocalStorageEntry, 32> LocalStorage;

  private:
    struct ActivationImpPrivate {
        ActivationImpPrivate(FunctionImp* f, const List& a)
            : function(f)
            , arguments(a)
            , argumentsObject(0)
        {
            ASSERT(f);
        }
        
        FunctionImp* function;
        LocalStorage localStorage;

        List arguments;
        Arguments* argumentsObject;
    };

  public:
    ActivationImp(FunctionImp* function, const List& arguments);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    virtual void mark();

    bool isActivation() { return true; }

    void releaseArguments() { d->arguments.reset(); }
    
    LocalStorage& localStorage() { return d->localStorage; }
    SymbolTable& symbolTable() { return *m_symbolTable; }

  private:
    static PropertySlot::GetValueFunc getArgumentsGetter();
    static JSValue* argumentsGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot);
    void createArgumentsObject(ExecState*);
    
    OwnPtr<ActivationImpPrivate> d;
    SymbolTable* m_symbolTable;
  };

  class GlobalFuncImp : public InternalFunctionImp {
  public:
    GlobalFuncImp(ExecState*, FunctionPrototype*, int i, int len, const Identifier&);
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List& args);
    enum { Eval, ParseInt, ParseFloat, IsNaN, IsFinite, Escape, UnEscape,
           DecodeURI, DecodeURIComponent, EncodeURI, EncodeURIComponent
#ifndef NDEBUG
           , KJSPrint
#endif
};
  private:
    int id;
  };

  static const double mantissaOverflowLowerBound = 9007199254740992.0;
  double parseIntOverflow(const char* s, int length, int radix);

} // namespace

#endif
