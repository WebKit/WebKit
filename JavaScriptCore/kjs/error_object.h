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

#ifndef _ERROR_OBJECT_H_
#define _ERROR_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class ErrorInstanceImp : public ObjectImp {
  public:
    ErrorInstanceImp(ObjectImp *proto);
    
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };
  
  class ErrorPrototypeImp : public ObjectImp {
  public:
    ErrorPrototypeImp(ExecState *exec,
                      ObjectPrototypeImp *objectProto,
                      FunctionPrototypeImp *funcProto);
  };

  class ErrorProtoFuncImp : public InternalFunctionImp {
  public:
    ErrorProtoFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
  };

  class ErrorObjectImp : public InternalFunctionImp {
  public:
    ErrorObjectImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                   ErrorPrototypeImp *errorProto);

    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);

    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
  };

  class NativeErrorPrototypeImp : public ObjectImp {
  public:
    NativeErrorPrototypeImp(ExecState *exec, ErrorPrototypeImp *errorProto,
                            ErrorType et, UString name, UString message);
  private:
    ErrorType errType;
  };

  class NativeErrorImp : public InternalFunctionImp {
  public:
    NativeErrorImp(ExecState *exec, FunctionPrototypeImp *funcProto,
                   ObjectImp *prot);

    virtual bool implementsConstruct() const;
    virtual ObjectImp *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);

    virtual void mark();

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    ObjectImp *proto;
  };

} // namespace

#endif
