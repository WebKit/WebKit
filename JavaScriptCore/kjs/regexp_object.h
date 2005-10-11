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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _REGEXP_OBJECT_H_
#define _REGEXP_OBJECT_H_

#include "internal.h"
#include "function_object.h"
#include "regexp.h"

namespace KJS {
  class ExecState;
  class RegExpPrototypeImp : public ObjectImp {
  public:
    RegExpPrototypeImp(ExecState *exec,
                       ObjectPrototypeImp *objProto,
                       FunctionPrototypeImp *funcProto);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class RegExpProtoFuncImp : public InternalFunctionImp {
  public:
    RegExpProtoFuncImp(ExecState *exec,
                       FunctionPrototypeImp *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    enum { Exec, Test, ToString };
  private:
    int id;
  };

  class RegExpImp : public ObjectImp {
  public:
    RegExpImp(RegExpPrototypeImp *regexpProto);
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
                    FunctionPrototypeImp *funcProto,
                    RegExpPrototypeImp *regProto);
    virtual ~RegExpObjectImp();
    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    virtual void put(ExecState *, const Identifier &, ValueImp *, int attr = None);
    void putValueProperty(ExecState *, int token, ValueImp *, int attr);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *, int token) const;
    UString performMatch(RegExp *, const UString&, int startOffset = 0, int *endOffset = 0, int **ovector = 0);
    ObjectImp *arrayOfMatches(ExecState *exec, const UString &result) const;
    
    virtual const ClassInfo *classInfo() const { return &info; }
  private:
    ValueImp *getBackref(unsigned) const;
    ValueImp *getLastMatch() const;
    ValueImp *getLastParen() const;
    ValueImp *getLeftContext() const;
    ValueImp *getRightContext() const;

    // Global search cache / settings
    bool multiline;
    UString lastInput;
    int *lastOvector;
    unsigned lastNumSubPatterns;
    
    static const ClassInfo info;
  };

} // namespace

#endif
