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

#include "JSFunction.h"
#include "JSWrapperObject.h"
#include "lookup.h"

namespace KJS {

    struct GregorianDateTime;
    class FunctionPrototype;
    class ObjectPrototype;

    class DateInstance : public JSWrapperObject {
    public:
        DateInstance(JSObject *proto);
        virtual ~DateInstance();
        
        bool getTime(GregorianDateTime&, int& offset) const;
        bool getUTCTime(GregorianDateTime&) const;
        bool getTime(double& milli, int& offset) const;
        bool getUTCTime(double& milli) const;
        
        virtual const ClassInfo *classInfo() const { return &info; }
        static const ClassInfo info;

        void msToGregorianDateTime(double, bool outputIsUTC, GregorianDateTime&) const;

    private:
        struct Cache;
        mutable Cache* m_cache;
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

    JSValue* dateProtoFuncToString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToUTCString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToDateString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToTimeString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToLocaleString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToLocaleDateString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToLocaleTimeString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncValueOf(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetTime(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetFullYear(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCFullYear(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncToGMTString(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetMonth(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCMonth(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetDate(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCDate(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetDay(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCDay(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetHours(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCHours(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetMinutes(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCMinutes(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetMilliSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetUTCMilliseconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetTimezoneOffset(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetTime(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetMilliSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCMilliseconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCSeconds(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetMinutes(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCMinutes(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetHours(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCHours(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetDate(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCDate(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetMonth(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCMonth(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetFullYear(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetUTCFullYear(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncSetYear(ExecState*, JSObject*, const ArgList&);
    JSValue* dateProtoFuncGetYear(ExecState*, JSObject*, const ArgList&);

    /**
     * @internal
     *
     * The initial value of the the global variable's "Date" property
     */
    class DateConstructor : public InternalFunction {
    public:
        DateConstructor(ExecState*, FunctionPrototype*, DatePrototype*);

        virtual ConstructType getConstructData(ConstructData&);
        virtual JSObject* construct(ExecState*, const ArgList& args);

        virtual JSValue* callAsFunction(ExecState*, JSObject* thisObj, const ArgList& args);

        JSObject* construct(const ArgList&);
    };

} // namespace

#endif
