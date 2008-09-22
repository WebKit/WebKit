/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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
#include "Arguments.h"

#include "JSActivation.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(Arguments);

const ClassInfo Arguments::info = { "Arguments", 0, 0, 0 };

struct ArgumentsData {
    ArgumentsData(JSActivation* activation, unsigned numParameters, unsigned firstArgumentIndex, unsigned numArguments)
        : activation(activation)
        , numParameters(numParameters)
        , firstArgumentIndex(firstArgumentIndex)
        , numArguments(numArguments)
    {
    }

    JSActivation* activation;
    unsigned numParameters;
    unsigned firstArgumentIndex;
    unsigned numArguments;
    OwnArrayPtr<JSValue*> extraArguments;
    OwnArrayPtr<bool> deletedArguments;
};

// ECMA 10.1.8
Arguments::Arguments(ExecState* exec, JSFunction* function, JSActivation* activation, int firstArgumentIndex, Register* argv, int argc)
    : JSObject(exec->lexicalGlobalObject()->argumentsStructure())
    , d(new ArgumentsData(activation, function->numParameters(), firstArgumentIndex, argc))
{
    ASSERT(activation);

    putDirect(exec->propertyNames().callee, function, DontEnum);
    putDirect(exec->propertyNames().length, jsNumber(exec, argc), DontEnum);
  
    if (d->numArguments > d->numParameters) {
        unsigned numExtraArguments = d->numArguments - d->numParameters;
        d->extraArguments.set(new JSValue*[numExtraArguments]);
        for (unsigned i = 0; i < numExtraArguments; ++i)
            d->extraArguments[i] = argv[d->numParameters + i].getJSValue();
    }
}

Arguments::~Arguments()
{
}

void Arguments::mark() 
{
    JSObject::mark();

    unsigned numExtraArguments = d->numArguments - d->numParameters;
    for (unsigned i = 0; i < numExtraArguments; ++i) {
        if (!d->extraArguments[i]->marked())
            d->extraArguments[i]->mark();
    }

    if (!d->activation->marked())
        d->activation->mark();
}

bool Arguments::getOwnPropertySlot(ExecState* exec, unsigned i, PropertySlot& slot)
{
    if (i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->activation->uncheckedSymbolTableGet(d->firstArgumentIndex + i, slot);
        else
            slot.setValueSlot(&d->extraArguments[i - d->numParameters]);
        return true;
    }

    return JSObject::getOwnPropertySlot(exec, Identifier(exec, UString::from(i)), slot);
}

bool Arguments::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->activation->uncheckedSymbolTableGet(d->firstArgumentIndex + i, slot);
        else
            slot.setValueSlot(&d->extraArguments[i - d->numParameters]);
        return true;
    }

    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Arguments::put(ExecState* exec, unsigned i, JSValue* value, PutPropertySlot& slot)
{
    if (i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->activation->uncheckedSymbolTablePut(d->firstArgumentIndex + i, value);
        else
            d->extraArguments[i - d->numParameters] = value;
        return;
    }

    JSObject::put(exec, Identifier(exec, UString::from(i)), value, slot);
}

void Arguments::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments && (!d->deletedArguments || !d->deletedArguments[i])) {
        if (i < d->numParameters)
            d->activation->uncheckedSymbolTablePut(d->firstArgumentIndex + i, value);
        else
            d->extraArguments[i - d->numParameters] = value;
        return;
    }

    JSObject::put(exec, propertyName, value, slot);
}

bool Arguments::deleteProperty(ExecState* exec, unsigned i) 
{
    if (i < d->numArguments) {
        if (!d->deletedArguments) {
            d->deletedArguments.set(new bool[d->numArguments]);
            memset(d->deletedArguments.get(), 0, sizeof(bool) * d->numArguments);
        }
        if (!d->deletedArguments[i]) {
            d->deletedArguments[i] = true;
            return true;
        }
    }

    return JSObject::deleteProperty(exec, Identifier(exec, UString::from(i)));
}

bool Arguments::deleteProperty(ExecState* exec, const Identifier& propertyName) 
{
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(&isArrayIndex);
    if (isArrayIndex && i < d->numArguments) {
        if (!d->deletedArguments) {
            d->deletedArguments.set(new bool[d->numArguments]);
            memset(d->deletedArguments.get(), 0, sizeof(bool) * d->numArguments);
        }
        if (!d->deletedArguments[i]) {
            d->deletedArguments[i] = true;
            return true;
        }
    }

    return JSObject::deleteProperty(exec, propertyName);
}

} // namespace JSC
