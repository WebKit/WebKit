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

#ifndef BOOL_OBJECT_H_
#define BOOL_OBJECT_H_

#include "function_object.h"
#include "JSWrapperObject.h"

namespace KJS {

    class BooleanInstance : public JSWrapperObject {
    public:
        BooleanInstance(JSObject* proto);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of Boolean.prototype (and thus all objects created
     * with the Boolean constructor
     */
    class BooleanPrototype : public BooleanInstance {
    public:
        BooleanPrototype(ExecState*, ObjectPrototype*, FunctionPrototype*);
    };

    /**
     * @internal
     *
     * The initial value of the the global variable's "Boolean" property
     */
    class BooleanObjectImp : public InternalFunctionImp {
    public:
        BooleanObjectImp(ExecState*, FunctionPrototype*, BooleanPrototype*);

        virtual bool implementsConstruct() const;
        virtual JSObject* construct(ExecState*, const List&);

        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
    };

} // namespace KJS

#endif // BOOL_OBJECT_H_
