// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJS_FUNCTION_H_
#define _KJS_FUNCTION_H_

#include "internal.h"
#include "array_instance.h"

namespace KJS {

  class Parameter;
  class ActivationImp;

  /**
   * @short Implementation class for internal Functions.
   */
  class FunctionImp : public InternalFunctionImp {
    friend class ActivationImp;
  public:
    FunctionImp(ExecState *exec, const Identifier &n = Identifier::null());
    virtual ~FunctionImp();

    virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);

    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    void addParameter(const Identifier &n);
    Identifier getParameterName(int index);
    // parameters in string representation, e.g. (a, b, c)
    UString parameterString() const;
    virtual CodeType codeType() const = 0;

    virtual Completion execute(ExecState *exec) = 0;
    Identifier name() const { return ident; }

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  protected:
    Parameter *param;
    Identifier ident;

  private:
    static ValueImp *argumentsGetter(ExecState *, const Identifier &, const PropertySlot&);
    static ValueImp *lengthGetter(ExecState *, const Identifier &, const PropertySlot&);

    void processParameters(ExecState *exec, const List &);
    virtual void processVarDecls(ExecState *exec);
  };

  class DeclaredFunctionImp : public FunctionImp {
  public:
    DeclaredFunctionImp(ExecState *exec, const Identifier &n,
			FunctionBodyNode *b, const ScopeChain &sc);

    bool implementsConstruct() const;
    ObjectImp *construct(ExecState *exec, const List &args);

    virtual Completion execute(ExecState *exec);
    CodeType codeType() const { return FunctionCode; }
    RefPtr<FunctionBodyNode> body;

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
    void IndexToNameMap::unMap(const Identifier &index);
    
  private:
    IndexToNameMap(); // prevent construction w/o parameters
    int size;
    Identifier * _map;
  };
  
  class ArgumentsImp : public ObjectImp {
  public:
    ArgumentsImp(ExecState *exec, FunctionImp *func, const List &args, ActivationImp *act);
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    static ValueImp *mappedIndexGetter(ExecState *exec, const Identifier &, const PropertySlot& slot);

    ActivationImp *_activationObject; 
    mutable IndexToNameMap indexToNameMap;
  };

  class ActivationImp : public ObjectImp {
  public:
    ActivationImp(FunctionImp *function, const List &arguments);

    virtual bool getOwnPropertySlot(ExecState *exec, const Identifier &, PropertySlot&);
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    
    virtual void mark();

    bool isActivation() { return true; }
  private:
    static PropertySlot::GetValueFunc getArgumentsGetter();
    static ValueImp *argumentsGetter(ExecState *exec, const Identifier &, const PropertySlot& slot);
    void createArgumentsObject(ExecState *exec) const;
    
    FunctionImp *_function;
    List _arguments;
    mutable ArgumentsImp *_argumentsObject;
  };

  class GlobalFuncImp : public InternalFunctionImp {
  public:
    GlobalFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto, int i, int len);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
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
