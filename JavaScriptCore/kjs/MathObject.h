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

#ifndef MathObject_h
#define MathObject_h

#include "FunctionPrototype.h"
#include "lookup.h"

namespace KJS {

    class MathObject : public JSObject {
    public:
        MathObject(ExecState*, ObjectPrototype*);

        bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        enum { Euler, Ln2, Ln10, Log2E, Log10E, Pi, Sqrt1_2, Sqrt2 };
    };

    // Functions
    JSValue* mathProtoFuncAbs(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncACos(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncASin(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncATan(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncATan2(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncCeil(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncCos(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncExp(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncFloor(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncLog(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncMax(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncMin(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncPow(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncRandom(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncRound(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncSin(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncSqrt(ExecState*, JSObject*, const ArgList&);
    JSValue* mathProtoFuncTan(ExecState*, JSObject*, const ArgList&);

} // namespace KJS

#endif // MathObject_h
