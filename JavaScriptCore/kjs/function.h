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

#include "internal.h"
#include <kxmlcore/OwnPtr.h>

namespace KJS {

  class ActivationImp;
  class FunctionBodyNode;
  class Parameter;

  /**
   * @short Implementation class for internal Functions.
   */
  class FunctionImp : public InternalFunctionImp {
    friend class ActivationImp;
  public:
    FunctionImp(ExecState* exec, const Identifier& n, FunctionBodyNode* b);
    virtual ~FunctionImp();

    virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);

    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    void addParameter(const Identifier &n);
    Identifier getParameterName(int index);
    // parameters in string representation, e.g. (a, b, c)
    UString parameterString() const;
    virtual CodeType codeType() const = 0;

    virtual Completion execute(ExecState *exec) = 0;

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;

    RefPtr<FunctionBodyNode> body;

  protected:
    OwnPtr<Parameter> param;

  private:
    static JSValue *argumentsGetter(ExecState *, JSObject *, const Identifier &, const PropertySlot&);
    static JSValue *lengthGetter(ExecState *, JSObject *, const Identifier &, const PropertySlot&);

    void processParameters(ExecState *exec, const List &);
    virtual void processVarDecls(ExecState *exec);
  };

  class DeclaredFunctionImp : public FunctionImp {
  public:
    DeclaredFunctionImp(ExecState *exec, const Identifier &n,
                        FunctionBodyNode *b, const ScopeChain &sc);

    bool implementsConstruct() const;
    JSObject *construct(ExecState *exec, const List &args);

    virtual Completion execute(ExecState *exec);
    CodeType codeType() const { return FunctionCode; }

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;

  private:
    virtual void processVarDecls(ExecState *exec);
  };

  class IndexToNameMap {
  public:
    IndexToNameMap(FunctionImp *func, const List &args);
    ~IndexToNameMap();
    
    Identifier& operator[](int index);
    Identifier& operator[](const Identifier &indexIdentifier);
    bool isMapped(const Identifier &index) const;
    void unMap(const Identifier &index);
    
  private:
    IndexToNameMap(); // prevent construction w/o parameters
    int size;
    Identifier * _map;
  };
  
  class Arguments : public JSObject {
  public:
    Arguments(ExecState *exec, FunctionImp *func, const List &args, ActivationImp *act);
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static JSValue *mappedIndexGetter(ExecState *exec, JSObject *, const Identifier &, const PropertySlot& slot);

    ActivationImp *_activationObject; 
    mutable IndexToNameMap indexToNameMap;
  };

  class ActivationImp : public JSObject {
  public:
    ActivationImp(FunctionImp *function, const List &arguments);

    virtual bool getOwnPropertySlot(ExecState *exec, const Identifier &, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    
    virtual void mark();

    bool isActivation() { return true; }
  private:
    static PropertySlot::GetValueFunc getArgumentsGetter();
    static JSValue *argumentsGetter(ExecState *exec, JSObject *, const Identifier &, const PropertySlot& slot);
    void createArgumentsObject(ExecState *exec) const;
    
    FunctionImp *_function;
    List _arguments;
    mutable Arguments *_argumentsObject;
  };

  class GlobalFuncImp : public InternalFunctionImp {
  public:
    GlobalFuncImp(ExecState*, FunctionPrototype*, int i, int len, const Identifier&);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
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



} // namespace

#endif
