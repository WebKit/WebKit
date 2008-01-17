// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef FUNCTION_OBJECT_H_
#define FUNCTION_OBJECT_H_

#include "object_object.h"
#include "function.h"

namespace KJS {

    /**
     * @internal
     *
     * The initial value of Function.prototype (and thus all objects created
     * with the Function constructor)
     */
    class FunctionPrototype : public InternalFunctionImp {
    public:
        FunctionPrototype(ExecState*);

        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
    };

    /**
     * @internal
     *
     * The initial value of the the global variable's "Function" property
     */
    class FunctionObjectImp : public InternalFunctionImp {
    public:
        FunctionObjectImp(ExecState*, FunctionPrototype*);

        virtual bool implementsConstruct() const;
        virtual JSObject* construct(ExecState*, const List&);
        virtual JSObject* construct(ExecState*, const List&, const Identifier& functionName, const UString& sourceURL, int lineNumber);
        virtual JSValue* callAsFunction(ExecState*, JSObject*, const List&);
    };

} // namespace KJS

#endif // _FUNCTION_OBJECT_H_
