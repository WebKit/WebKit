// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "object.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace KJS {

  class ActivationImp;
  class FunctionBodyNode;
  class FunctionPrototype;

  enum CodeType { GlobalCode,
                  EvalCode,
                  FunctionCode,
                  AnonymousCode };

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

  /**
   * @short Implementation class for internal Functions.
   */
  class FunctionImp : public InternalFunctionImp {
    friend class ActivationImp;
  public:
    FunctionImp(ExecState*, const Identifier& n, FunctionBodyNode* b);
    virtual ~FunctionImp();

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List& args);

    // Note: unlike body->paramName, this returns Identifier::null for parameters 
    // that will never get set, due to later param having the same name
    Identifier getParameterName(int index);
    virtual CodeType codeType() const = 0;

    virtual Completion execute(ExecState*) = 0;

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    RefPtr<FunctionBodyNode> body;

    /**
     * Returns the scope of this object. This is used when execution declared
     * functions - the execution context for the function is initialized with
     * extra object in it's scope. An example of this is functions declared
     * inside other functions:
     *
     * \code
     * function f() {
     *
     *   function b() {
     *     return prototype;
     *   }
     *
     *   var x = 4;
     *   // do some stuff
     * }
     * f.prototype = new String();
     * \endcode
     *
     * When the function f.b is executed, its scope will include properties of
     * f. So in the example above the return value of f.b() would be the new
     * String object that was assigned to f.prototype.
     *
     * @param exec The current execution state
     * @return The function's scope
     */
    const ScopeChain& scope() const { return _scope; }
    void setScope(const ScopeChain& s) { _scope = s; }

    virtual void mark();

  private:
    ScopeChain _scope;

    static JSValue* argumentsGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* callerGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
    static JSValue* lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);

    void passInParameters(ExecState*, const List&);
    virtual void processVarDecls(ExecState*);
  };

  class DeclaredFunctionImp : public FunctionImp {
  public:
    DeclaredFunctionImp(ExecState*, const Identifier& n,
                        FunctionBodyNode* b, const ScopeChain& sc);

    bool implementsConstruct() const;
    JSObject* construct(ExecState*, const List& args);

    virtual Completion execute(ExecState*);
    CodeType codeType() const { return FunctionCode; }

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

  private:
    virtual void processVarDecls(ExecState*);
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
    ActivationImp(FunctionImp* function, const List& arguments);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value, int attr = None);
    virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    virtual void mark();

    bool isActivation() { return true; }

    void releaseArguments() { _arguments.reset(); }

  private:
    static PropertySlot::GetValueFunc getArgumentsGetter();
    static JSValue* argumentsGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot);
    void createArgumentsObject(ExecState*);

    FunctionImp* _function;
    List _arguments;
    mutable Arguments* _argumentsObject;
  };

  class GlobalFuncImp : public InternalFunctionImp {
  public:
    GlobalFuncImp(ExecState*, FunctionPrototype*, int i, int len, const Identifier&);
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const List& args);
    virtual CodeType codeType() const;
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

UString escapeStringForPrettyPrinting(const UString& s);

} // namespace

#endif
