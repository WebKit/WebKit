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

#include "function.h"
#include "JSWrapperObject.h"
#include "lookup.h"

namespace KJS {

    struct GregorianDateTime;

    class FunctionPrototype;
    class ObjectPrototype;

    class DateInstance : public JSWrapperObject {
    public:
        DateInstance(JSObject *proto);
        
        bool getTime(GregorianDateTime&, int& offset) const;
        bool getUTCTime(GregorianDateTime&) const;
        bool getTime(double& milli, int& offset) const;
        bool getUTCTime(double& milli) const;
        
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
     * Functions to implement all methods that are properties of the
     * Date.prototype object
     */
    
    // Non-normative properties (Appendix B)
    // GetYear, SetYear, ToGMTString

    JSValue* dateProtoFuncToString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToUTCString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToDateString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToTimeString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToLocaleString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToLocaleDateString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncValueOf(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetTime(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetFullYear(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncToGMTString(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetMonth(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCMonth(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetDate(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCDate(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetDay(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCDay(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetHours(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCHours(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetMinutes(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetTime(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetMinutes(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetHours(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCHours(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetDate(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCDate(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetMonth(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCMonth(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetFullYear(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncSetYear(ExecState*, JSObject*, const List&);
    JSValue* dateProtoFuncGetYear(ExecState*, JSObject*, const List&);

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

        JSObject *construct(const List &);
    };

} // namespace

#endif
