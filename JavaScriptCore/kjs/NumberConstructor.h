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

#ifndef NumberConstructor_h
#define NumberConstructor_h

#include "JSFunction.h"

namespace KJS {

    class FunctionPrototype;
    class NumberPrototype;

    /**
     * @internal
     *
     * The initial value of the the global variable's "Number" property
     */
    class NumberConstructor : public InternalFunction {
    public:
        NumberConstructor(ExecState*, FunctionPrototype*, NumberPrototype*);

        bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        JSValue* getValueProperty(ExecState*, int token) const;

        static const ClassInfo info;

        enum { NaNValue, NegInfinity, PosInfinity, MaxValue, MinValue };

    private:
        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);
        virtual const ClassInfo* classInfo() const { return &info; }
    };

} // namespace KJS

#endif // NumberConstructor_h
