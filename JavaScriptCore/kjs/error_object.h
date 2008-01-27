/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ERROR_OBJECT_H_
#define ERROR_OBJECT_H_

#include "function_object.h"

namespace KJS {

    class ErrorInstance : public JSObject {
    public:
        ErrorInstance(JSObject* prototype);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    class ErrorPrototype : public ErrorInstance {
    public:
        ErrorPrototype(ExecState*, ObjectPrototype*, FunctionPrototype*);
    };

    JSValue* errorProtoFuncToString(ExecState*, JSObject*, const List&);

    class ErrorObjectImp : public InternalFunctionImp {
    public:
        ErrorObjectImp(ExecState*, FunctionPrototype*, ErrorPrototype*);

        virtual bool implementsConstruct() const;
        virtual JSObject* construct(ExecState*, const List&);

        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
    };

    class NativeErrorPrototype : public JSObject {
    public:
        NativeErrorPrototype(ExecState*, ErrorPrototype*, const UString& name, const UString& message);
    };

    class NativeErrorImp : public InternalFunctionImp {
    public:
        NativeErrorImp(ExecState*, FunctionPrototype*, NativeErrorPrototype*);

        virtual bool implementsConstruct() const;
        virtual JSObject* construct(ExecState*, const List&);
        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);

        virtual void mark();

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

    private:
        JSObject* proto;
    };

} // namespace KJS

#endif // ERROR_OBJECT_H_
