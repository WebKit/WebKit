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

#ifndef MATH_OBJECT_H_
#define MATH_OBJECT_H_

#include "function_object.h"

namespace KJS {

  class MathObjectImp : public JSObject {
  public:
    MathObjectImp(ExecState *exec,
                  ObjectPrototype *objProto);
    bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Euler, Ln2, Ln10, Log2E, Log10E, Pi, Sqrt1_2, Sqrt2,
           Abs, ACos, ASin, ATan, ATan2, Ceil, Cos, Pow,
           Exp, Floor, Log, Max, Min, Random, Round, Sin, Sqrt, Tan };
  };

  class MathFuncImp : public InternalFunctionImp {
  public:
    MathFuncImp(ExecState *exec, int i, int l, const Identifier&);
    virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
  private:
    int id;
  };

} // namespace

#endif
