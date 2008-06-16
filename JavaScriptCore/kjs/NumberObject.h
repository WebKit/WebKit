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

#ifndef NumberObject_h
#define NumberObject_h

#include "FunctionPrototype.h"
#include "JSWrapperObject.h"

namespace KJS {

    class NumberObject : public JSWrapperObject {
    public:
        NumberObject(JSObject* prototype);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of Number.prototype (and thus all objects created
     * with the Number constructor
     */
    class NumberPrototype : public NumberObject {
    public:
        NumberPrototype(ExecState*, ObjectPrototype*, FunctionPrototype*);
    };

    /**
     * @internal
     *
     * The initial value of the the global variable's "Number" property
     */
    class NumberConstructor : public InternalFunction {
    public:
        NumberConstructor(ExecState*, FunctionPrototype*, NumberPrototype*);

        virtual ConstructType getConstructData(ConstructData&);
        virtual JSObject* construct(ExecState*, const List&);

        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);

        bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        enum { NaNValue, NegInfinity, PosInfinity, MaxValue, MinValue };

        JSObject* construct(const List&);
    };

} // namespace KJS

#endif // NumberObject_h
