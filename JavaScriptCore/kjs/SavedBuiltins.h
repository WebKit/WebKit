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

namespace KJS {
    
class SavedBuiltinsInternal;

class SavedBuiltins {
    friend class Interpreter;
public:
    SavedBuiltins();
    ~SavedBuiltins();
private:
    SavedBuiltinsInternal *_internal;
};

class SavedBuiltinsInternal {
    friend class Interpreter;
private:
    ProtectedPtr<ObjectObjectImp> m_Object;
    ProtectedPtr<FunctionObjectImp> m_Function;
    ProtectedPtr<ArrayObjectImp> m_Array;
    ProtectedPtr<BooleanObjectImp> m_Boolean;
    ProtectedPtr<StringObjectImp> m_String;
    ProtectedPtr<NumberObjectImp> m_Number;
    ProtectedPtr<DateObjectImp> m_Date;
    ProtectedPtr<RegExpObjectImp> m_RegExp;
    ProtectedPtr<ErrorObjectImp> m_Error;
    
    ProtectedPtr<ObjectPrototype> m_ObjectPrototype;
    ProtectedPtr<FunctionPrototype> m_FunctionPrototype;
    ProtectedPtr<ArrayPrototype> m_ArrayPrototype;
    ProtectedPtr<BooleanPrototype> m_BooleanPrototype;
    ProtectedPtr<StringPrototype> m_StringPrototype;
    ProtectedPtr<NumberPrototype> m_NumberPrototype;
    ProtectedPtr<DatePrototype> m_DatePrototype;
    ProtectedPtr<RegExpPrototype> m_RegExpPrototype;
    ProtectedPtr<ErrorPrototype> m_ErrorPrototype;
    
    ProtectedPtr<NativeErrorImp> m_EvalError;
    ProtectedPtr<NativeErrorImp> m_RangeError;
    ProtectedPtr<NativeErrorImp> m_ReferenceError;
    ProtectedPtr<NativeErrorImp> m_SyntaxError;
    ProtectedPtr<NativeErrorImp> m_TypeError;
    ProtectedPtr<NativeErrorImp> m_UriError;
    
    ProtectedPtr<NativeErrorPrototype> m_EvalErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> m_RangeErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> m_ReferenceErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> m_SyntaxErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> m_TypeErrorPrototype;
    ProtectedPtr<NativeErrorPrototype> m_UriErrorPrototype;
};

} // namespace

#endif // SavedBuiltins_H
