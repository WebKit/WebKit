// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef SavedBuiltins_H
#define SavedBuiltins_H

#include "protect.h"
#include "object_object.h"
#include "string_object.h"
#include "error_object.h"
#include "regexp_object.h"
#include "array_object.h"
#include "bool_object.h"
#include "date_object.h"
#include "number_object.h"
#include "math_object.h"

namespace KJS {
    
struct SavedBuiltinsInternal {
    ProtectedPtr<ObjectObjectImp> objectConstructor;
    ProtectedPtr<FunctionObjectImp> functionConstructor;
    ProtectedPtr<ArrayObjectImp> arrayConstructor;
    ProtectedPtr<BooleanObjectImp> booleanConstructor;
    ProtectedPtr<StringObjectImp> stringConstructor;
    ProtectedPtr<NumberObjectImp> numberConstructor;
    ProtectedPtr<DateObjectImp> dateConstructor;
    ProtectedPtr<RegExpObjectImp> regExpConstructor;
    ProtectedPtr<ErrorObjectImp> errorConstructor;
    ProtectedPtr<NativeErrorImp> evalErrorConstructor;
    ProtectedPtr<NativeErrorImp> rangeErrorConstructor;
    ProtectedPtr<NativeErrorImp> referenceErrorConstructor;
    ProtectedPtr<NativeErrorImp> syntaxErrorConstructor;
    ProtectedPtr<NativeErrorImp> typeErrorConstructor;
    ProtectedPtr<NativeErrorImp> URIErrorConstructor;
    
    ProtectedPtr<ObjectPrototype> objectPrototype;
    ProtectedPtr<FunctionPrototype> functionPrototype;
    ProtectedPtr<ArrayPrototype> arrayPrototype;
    ProtectedPtr<BooleanPrototype> booleanPrototype;
    ProtectedPtr<StringPrototype> stringPrototype;
    ProtectedPtr<NumberPrototype> numberPrototype;
    ProtectedPtr<DatePrototype> datePrototype;
    ProtectedPtr<RegExpPrototype> regExpPrototype;
    ProtectedPtr<ErrorPrototype> errorPrototype;
    ProtectedPtr<NativeErrorPrototype> evalErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> rangeErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> referenceErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> syntaxErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> typeErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> URIErrorPrototype;
};

class SavedBuiltins {
    friend class JSGlobalObject;
public:
    SavedBuiltins()
        : _internal(0)
    {
    }

    ~SavedBuiltins()
    {
        delete _internal;
    }

private:
    SavedBuiltinsInternal* _internal;
};

} // namespace

#endif // SavedBuiltins_H
