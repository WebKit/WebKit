// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _REGEXP_OBJECT_H_
#define _REGEXP_OBJECT_H_

#include "internal.h"
#include "function_object.h"
#include "regexp.h"

namespace KJS {
  class ExecState;
  class RegExpPrototype : public JSObject {
  public:
    RegExpPrototype(ExecState *exec,
                       ObjectPrototype *objProto,
                       FunctionPrototype *funcProto);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class RegExpProtoFunc : public InternalFunctionImp {
  public:
    RegExpProtoFunc(ExecState *exec,
                       FunctionPrototype *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { Exec, Test, ToString };
  private:
    int id;
  };

  class RegExpImp : public JSObject {
  public:
    RegExpImp(RegExpPrototype *regexpProto);
    ~RegExpImp();
    void setRegExp(RegExp *r) { reg = r; }
    RegExp* regExp() const { return reg; }

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    RegExp *reg;
  };

  class RegExpObjectImp : public InternalFunctionImp {
  public:
    enum { Dollar1, Dollar2, Dollar3, Dollar4, Dollar5, Dollar6, Dollar7, Dollar8, Dollar9, 
           Input, Multiline, LastMatch, LastParen, LeftContext, RightContext };
    
    RegExpObjectImp(ExecState *exec,
                    FunctionPrototype *funcProto,
                    RegExpPrototype *regProto);
    virtual ~RegExpObjectImp();
    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    virtual void put(ExecState *, const Identifier &, JSValue *, int attr = None);
    void putValueProperty(ExecState *, int token, JSValue *, int attr);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *, int token) const;
    UString performMatch(RegExp *, const UString&, int startOffset = 0, int *endOffset = 0, int **ovector = 0);
    JSObject *arrayOfMatches(ExecState *exec, const UString &result) const;
    
    virtual const ClassInfo *classInfo() const { return &info; }
  private:
    JSValue *getBackref(unsigned) const;
    JSValue *getLastMatch() const;
    JSValue *getLastParen() const;
    JSValue *getLeftContext() const;
    JSValue *getRightContext() const;

    // Global search cache / settings
    bool multiline;
    UString lastInput;
    int *lastOvector;
    unsigned lastNumSubPatterns;
    
    static const ClassInfo info;
  };

} // namespace

#endif
