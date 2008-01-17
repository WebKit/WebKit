/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef ARRAY_OBJECT_H_
#define ARRAY_OBJECT_H_

#include "array_instance.h"
#include "function_object.h"
#include "lookup.h"

namespace KJS {

 class ArrayPrototype : public ArrayInstance {
  public:
    ArrayPrototype(ExecState*, ObjectPrototype*);

    bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class ArrayObjectImp : public InternalFunctionImp {
  public:
    ArrayObjectImp(ExecState*, FunctionPrototype*, ArrayPrototype*);

    virtual bool implementsConstruct() const;
    virtual JSObject* construct(ExecState*, const List&);
    virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);

  };

  JSValue* arrayProtoFuncToString(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncToLocaleString(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncConcat(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncJoin(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncPop(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncPush(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncReverse(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncShift(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncSlice(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncSort(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncSplice(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncUnShift(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncEvery(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncForEach(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncSome(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncIndexOf(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncFilter(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncMap(ExecState*, JSObject*, const List&);
  JSValue* arrayProtoFuncLastIndexOf(ExecState*, JSObject*, const List&);

} // namespace KJS

#endif // ARRAY_OBJECT_H_
