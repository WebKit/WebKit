// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef _KJS_FUNCTION_H_
#define _KJS_FUNCTION_H_

#include "internal.h"

namespace KJS {

  class Parameter;

  /**
   * @short Implementation class for internal Functions.
   */
  class FunctionImp : public InternalFunctionImp {
    friend class Function;
    friend class ActivationImp;
  public:
    FunctionImp(ExecState *exec, const UString &n = UString::null);
    virtual ~FunctionImp();

    virtual void mark();

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);

    void addParameter(const UString &n);
    // parameters in string representation, e.g. (a, b, c)
    UString parameterString() const;
    virtual CodeType codeType() const = 0;

    virtual Completion execute(ExecState *exec) = 0;
    UString name() const { return ident; }

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  protected:
    Parameter *param;
    UString ident;

  private:
    void processParameters(ExecState *exec, const List &);
    virtual void processVarDecls(ExecState *exec);
  };

  class DeclaredFunctionImp : public FunctionImp {
  public:
    DeclaredFunctionImp(ExecState *exec, const UString &n,
			FunctionBodyNode *b, const List &sc);
    ~DeclaredFunctionImp();

    bool implementsConstruct() const;
    Object construct(ExecState *exec, const List &args);

    virtual Completion execute(ExecState *exec);
    CodeType codeType() const { return FunctionCode; }
    FunctionBodyNode *body;

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    virtual void processVarDecls(ExecState *exec);
  };




  class ArgumentsImp : public ObjectImp {
  public:
    ArgumentsImp(ExecState *exec, FunctionImp *func, const List &args);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class ActivationImp : public ObjectImp {
  public:
    ActivationImp(ExecState *exec, FunctionImp *f, const List &args);
    ~ActivationImp();

    Object argumentsObject() { return Object(arguments); }

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    ObjectImp* arguments;
  };

  class GlobalFuncImp : public InternalFunctionImp {
  public:
    GlobalFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto, int i, int len);
    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
    virtual CodeType codeType() const;
    enum { Eval, ParseInt, ParseFloat, IsNaN, IsFinite, Escape, UnEscape };
  private:
    int id;
  };



}; // namespace

#endif
