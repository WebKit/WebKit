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
    ProtectedPtr<JSObject> m_Object;
    ProtectedPtr<JSObject> m_Function;
    ProtectedPtr<JSObject> m_Array;
    ProtectedPtr<JSObject> m_Boolean;
    ProtectedPtr<JSObject> m_String;
    ProtectedPtr<JSObject> m_Number;
    ProtectedPtr<JSObject> m_Date;
    ProtectedPtr<JSObject> m_RegExp;
    ProtectedPtr<JSObject> m_Error;
    
    ProtectedPtr<JSObject> m_ObjectPrototype;
    ProtectedPtr<JSObject> m_FunctionPrototype;
    ProtectedPtr<JSObject> m_ArrayPrototype;
    ProtectedPtr<JSObject> m_BooleanPrototype;
    ProtectedPtr<JSObject> m_StringPrototype;
    ProtectedPtr<JSObject> m_NumberPrototype;
    ProtectedPtr<JSObject> m_DatePrototype;
    ProtectedPtr<JSObject> m_RegExpPrototype;
    ProtectedPtr<JSObject> m_ErrorPrototype;
    
    ProtectedPtr<JSObject> m_EvalError;
    ProtectedPtr<JSObject> m_RangeError;
    ProtectedPtr<JSObject> m_ReferenceError;
    ProtectedPtr<JSObject> m_SyntaxError;
    ProtectedPtr<JSObject> m_TypeError;
    ProtectedPtr<JSObject> m_UriError;
    
    ProtectedPtr<JSObject> m_EvalErrorPrototype;
    ProtectedPtr<JSObject> m_RangeErrorPrototype;
    ProtectedPtr<JSObject> m_ReferenceErrorPrototype;
    ProtectedPtr<JSObject> m_SyntaxErrorPrototype;
    ProtectedPtr<JSObject> m_TypeErrorPrototype;
    ProtectedPtr<JSObject> m_UriErrorPrototype;
};

} // namespace

#endif // SavedBuiltins_H
