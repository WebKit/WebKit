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

#ifndef DATE_OBJECT_H
#define DATE_OBJECT_H

#include "internal.h"

namespace KJS {

    class FunctionPrototype;
    class ObjectPrototype;

    class DateInstance : public JSObject {
    public:
        DateInstance(JSObject *proto);
        
        bool getTime(tm &t, int &gmtoffset) const;
        bool getUTCTime(tm &t) const;
        bool getTime(double &ms, int &gmtoffset) const;
        bool getUTCTime(double &ms) const;
        
        virtual const ClassInfo *classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of Date.prototype (and thus all objects created
     * with the Date constructor
     */
    class DatePrototype : public DateInstance {
    public:
        DatePrototype(ExecState *, ObjectPrototype *);
        virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
        virtual const ClassInfo *classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of the the global variable's "Date" property
     */
    class DateObjectImp : public InternalFunctionImp {
    public:
        DateObjectImp(ExecState *, FunctionPrototype *, DatePrototype *);

        virtual bool implementsConstruct() const;
        virtual JSObject *construct(ExecState *, const List &args);
        virtual JSValue *callAsFunction(ExecState *, JSObject *thisObj, const List &args);

        Completion execute(const List &);
        JSObject *construct(const List &);
    };

} // namespace

#endif
