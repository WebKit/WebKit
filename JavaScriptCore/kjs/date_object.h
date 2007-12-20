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
     * Class to implement all methods that are properties of the
     * Date.prototype object
     */
    
    // Non-normative properties (Appendix B)
    // GetYear, SetYear, ToGMTString

#define FOR_EACH_CLASS(macro) \
    macro(DateProtoFuncToString) \
    macro(DateProtoFuncToUTCString) \
    macro(DateProtoFuncToDateString) \
    macro(DateProtoFuncToTimeString) \
    macro(DateProtoFuncToLocaleString) \
    macro(DateProtoFuncToLocaleDateString) \
    macro(DateProtoFuncToLocaleTimeString) \
    macro(DateProtoFuncValueOf) \
    macro(DateProtoFuncGetTime) \
    macro(DateProtoFuncGetFullYear) \
    macro(DateProtoFuncGetUTCFullYear) \
    macro(DateProtoFuncToGMTString) \
    macro(DateProtoFuncGetMonth) \
    macro(DateProtoFuncGetUTCMonth) \
    macro(DateProtoFuncGetDate) \
    macro(DateProtoFuncGetUTCDate) \
    macro(DateProtoFuncGetDay) \
    macro(DateProtoFuncGetUTCDay) \
    macro(DateProtoFuncGetHours) \
    macro(DateProtoFuncGetUTCHours) \
    macro(DateProtoFuncGetMinutes) \
    macro(DateProtoFuncGetUTCMinutes) \
    macro(DateProtoFuncGetSeconds) \
    macro(DateProtoFuncGetUTCSeconds) \
    macro(DateProtoFuncGetMilliSeconds) \
    macro(DateProtoFuncGetUTCMilliseconds) \
    macro(DateProtoFuncGetTimezoneOffset) \
    macro(DateProtoFuncSetTime) \
    macro(DateProtoFuncSetMilliSeconds) \
    macro(DateProtoFuncSetUTCMilliseconds) \
    macro(DateProtoFuncSetSeconds) \
    macro(DateProtoFuncSetUTCSeconds) \
    macro(DateProtoFuncSetMinutes) \
    macro(DateProtoFuncSetUTCMinutes) \
    macro(DateProtoFuncSetHours) \
    macro(DateProtoFuncSetUTCHours) \
    macro(DateProtoFuncSetDate) \
    macro(DateProtoFuncSetUTCDate) \
    macro(DateProtoFuncSetMonth) \
    macro(DateProtoFuncSetUTCMonth) \
    macro(DateProtoFuncSetFullYear) \
    macro(DateProtoFuncSetUTCFullYear) \
    macro(DateProtoFuncSetYear) \
    macro(DateProtoFuncGetYear) \

FOR_EACH_CLASS(KJS_IMPLEMENT_PROTOTYPE_FUNCTION_WITH_CREATE)
#undef FOR_EACH_CLASS


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
