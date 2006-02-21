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

#ifndef _ARRAY_OBJECT_H_
#define _ARRAY_OBJECT_H_

#include "array_instance.h"
#include "internal.h"
#include "function_object.h"

namespace KJS {

 class ArrayPrototype : public ArrayInstance {
  public:
    ArrayPrototype(ExecState *exec,
                      ObjectPrototype *objProto);
    bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class ArrayProtoFunc : public InternalFunctionImp {
  public:
    ArrayProtoFunc(ExecState *exec, int i, int len, const Identifier& name);

    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

    enum { ToString, ToLocaleString, Concat, Join, Pop, Push,
          Reverse, Shift, Slice, Sort, Splice, UnShift, 
          Every, ForEach, Some, IndexOf, Filter, Map };
  private:
    int id;
  };

  const unsigned MAX_ARRAY_INDEX = 0xFFFFFFFEu;

  class ArrayObjectImp : public InternalFunctionImp {
  public:
    ArrayObjectImp(ExecState *exec,
                   FunctionPrototype *funcProto,
                   ArrayPrototype *arrayProto);

    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

  };

} // namespace

#endif
