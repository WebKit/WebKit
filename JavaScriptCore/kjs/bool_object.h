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

#ifndef _BOOL_OBJECT_H_
#define _BOOL_OBJECT_H_

#include "internal.h"
#include "function_object.h"

namespace KJS {

  class BooleanInstance : public JSObject {
  public:
    BooleanInstance(JSObject *proto);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  /**
   * @internal
   *
   * The initial value of Boolean.prototype (and thus all objects created
   * with the Boolean constructor
   */
  class BooleanPrototype : public BooleanInstance {
  public:
    BooleanPrototype(ExecState *exec,
                        ObjectPrototype *objectProto,
                        FunctionPrototype *funcProto);
  };

  /**
   * @internal
   *
   * Class to implement all methods that are properties of the
   * Boolean.prototype object
   */
  class BooleanProtoFunc : public InternalFunctionImp {
  public:
    BooleanProtoFunc(ExecState *exec,
                        FunctionPrototype *funcProto, int i, int len);

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { ToString, ValueOf };
  private:
    int id;
  };

  /**
   * @internal
   *
   * The initial value of the the global variable's "Boolean" property
   */
  class BooleanObjectImp : public InternalFunctionImp {
    friend class BooleanProtoFunc;
  public:
    BooleanObjectImp(ExecState *exec, FunctionPrototype *funcProto,
                     BooleanPrototype *booleanProto);

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);

    virtual bool implementsCall() const;
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  };

} // namespace

#endif
