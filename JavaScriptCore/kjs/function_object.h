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

#ifndef _FUNCTION_OBJECT_H_
#define _FUNCTION_OBJECT_H_

#include "internal.h"
#include "object_object.h"
#include "function.h"

namespace KJS {

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

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Function.prototype object
   */
  class FunctionProtoFunc : public InternalFunctionImp {
  public:
    FunctionProtoFunc(ExecState *exec,
                        FunctionPrototype *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { ToString, Apply, Call };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Function" property
   */
  class FunctionObjectImp : public InternalFunctionImp {
  public:
    FunctionObjectImp(ExecState *exec, FunctionPrototype *funcProto);
    virtual ~FunctionObjectImp();

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args, const UString &sourceURL, int lineNumber);
    virtual JSObject *construct(ExecState *exec, const List &args);
    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

} // namespace

#endif // _FUNCTION_OBJECT_H_
