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

#ifndef ArrayPrototype_h
#define ArrayPrototype_h

#include "JSArray.h"
#include "FunctionPrototype.h"
#include "lookup.h"

namespace KJS {

 class ArrayPrototype : public JSArray {
  public:
    ArrayPrototype(ExecState*, ObjectPrototype*);

    bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class ArrayConstructor : public InternalFunction {
  public:
    ArrayConstructor(ExecState*, FunctionPrototype*, ArrayPrototype*);

    virtual ConstructType getConstructData(ConstructData&);
    virtual JSObject* construct(ExecState*, const ArgList&);

    virtual JSValue* callAsFunction(ExecState*, JSObject*, const ArgList&);

  };

  JSValue* arrayProtoFuncToString(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncToLocaleString(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncConcat(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncJoin(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncPop(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncPush(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncReverse(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncShift(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncSlice(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncSort(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncSplice(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncUnShift(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncEvery(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncForEach(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncSome(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncIndexOf(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncFilter(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncMap(ExecState*, JSObject*, const ArgList&);
  JSValue* arrayProtoFuncLastIndexOf(ExecState*, JSObject*, const ArgList&);

} // namespace KJS

#endif // ArrayPrototype_h
