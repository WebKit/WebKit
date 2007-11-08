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
#include "lookup.h"

namespace KJS {

  class MathObjectImp : public JSObject {
  public:
    MathObjectImp(ExecState*, ObjectPrototype*);

    bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue* getValueProperty(ExecState*, int token) const;

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    enum { Euler, Ln2, Ln10, Log2E, Log10E, Pi, Sqrt1_2, Sqrt2 };
  };


#define FOR_EACH_CLASS(macro) \
    macro(MathProtoFuncAbs) \
    macro(MathProtoFuncACos) \
    macro(MathProtoFuncASin) \
    macro(MathProtoFuncATan) \
    macro(MathProtoFuncATan2) \
    macro(MathProtoFuncCeil) \
    macro(MathProtoFuncCos) \
    macro(MathProtoFuncExp) \
    macro(MathProtoFuncFloor) \
    macro(MathProtoFuncLog) \
    macro(MathProtoFuncMax) \
    macro(MathProtoFuncMin) \
    macro(MathProtoFuncPow) \
    macro(MathProtoFuncRandom) \
    macro(MathProtoFuncRound) \
    macro(MathProtoFuncSin) \
    macro(MathProtoFuncSqrt) \
    macro(MathProtoFuncTan) \

FOR_EACH_CLASS(KJS_IMPLEMENT_PROTOTYPE_FUNCTION_WITH_CREATE)
#undef FOR_EACH_CLASS


} // namespace KJS

#endif // MATH_OBJECT_H_
