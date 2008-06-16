// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "config.h"
#include "operations.h"

#include "JSString.h"
#include "JSObject.h"
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if HAVE(FLOAT_H)
#include <float.h>
#endif

namespace KJS {

// ECMA 11.9.3
bool equal(ExecState *exec, JSValue *v1, JSValue *v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    
    if (t1 != t2) {
        if (t1 == UndefinedType)
            t1 = NullType;
        if (t2 == UndefinedType)
            t2 = NullType;
        
        if (t1 == BooleanType)
            t1 = NumberType;
        if (t2 == BooleanType)
            t2 = NumberType;
        
        if (t1 == NumberType && t2 == StringType) {
            // use toNumber
        } else if (t1 == StringType && t2 == NumberType)
            t1 = NumberType;
            // use toNumber
        else {
            if ((t1 == StringType || t1 == NumberType) && t2 == ObjectType) {
                v2 = v2->toPrimitive(exec);
                if (exec->hadException())
                    return false;
                return equal(exec, v1, v2);
            }
            if (t1 == NullType && t2 == ObjectType)
                return static_cast<JSObject *>(v2)->masqueradeAsUndefined();
            if (t1 == ObjectType && (t2 == StringType || t2 == NumberType)) {
                v1 = v1->toPrimitive(exec);
                if (exec->hadException())
                    return false;
                return equal(exec, v1, v2);
            }
            if (t1 == ObjectType && t2 == NullType)
                return static_cast<JSObject *>(v1)->masqueradeAsUndefined();
            if (t1 != t2)
                return false;
        }
    }
    
    if (t1 == UndefinedType || t1 == NullType)
        return true;
    
    if (t1 == NumberType) {
        double d1 = v1->toNumber(exec);
        double d2 = v2->toNumber(exec);
        return d1 == d2;
    }
    
    if (t1 == StringType)
        return static_cast<JSString*>(v1)->value() == static_cast<JSString*>(v2)->value();
    
    if (t1 == BooleanType)
        return v1->toBoolean(exec) == v2->toBoolean(exec);
    
    // types are Object
    return v1 == v2;
}

bool strictEqual(JSValue* v1, JSValue* v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    
    if (t1 != t2)
        return false;

    if (t1 == NumberType)
        return v1->getNumber() == v2->getNumber();
    
    if (t1 == StringType)
        return static_cast<JSString*>(v1)->value() == static_cast<JSString*>(v2)->value();
    
    return v1 == v2; // covers object, boolean, null, and undefined types
}

JSValue* throwOutOfMemoryError(ExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return error;
}

}
