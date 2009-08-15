/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Error_h
#define Error_h

#include <stdint.h>

namespace JSC {

    class ExecState;
    class JSObject;
    class UString;

    /**
     * Types of Native Errors available. For custom errors, GeneralError
     * should be used.
     */
    enum ErrorType {
        GeneralError   = 0,
        EvalError      = 1,
        RangeError     = 2,
        ReferenceError = 3,
        SyntaxError    = 4,
        TypeError      = 5,
        URIError       = 6
    };
    
    extern const char* expressionBeginOffsetPropertyName;
    extern const char* expressionCaretOffsetPropertyName;
    extern const char* expressionEndOffsetPropertyName;
    
    class Error {
    public:
        static JSObject* create(ExecState*, ErrorType, const UString& message, int lineNumber, intptr_t sourceID, const UString& sourceURL);
        static JSObject* create(ExecState*, ErrorType, const char* message);
    };

    JSObject* throwError(ExecState*, ErrorType, const UString& message, int lineNumber, intptr_t sourceID, const UString& sourceURL);
    JSObject* throwError(ExecState*, ErrorType, const UString& message);
    JSObject* throwError(ExecState*, ErrorType, const char* message);
    JSObject* throwError(ExecState*, ErrorType);
    JSObject* throwError(ExecState*, JSObject*);

} // namespace JSC

#endif // Error_h
