/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include <identifier.h>
#include <interpreter.h>
#include <runtime.h>
#include <runtime_array.h>

using namespace KJS;

const ClassInfo RuntimeArrayImp::info = {"RuntimeArray", &ArrayInstanceImp::info, 0, 0};

RuntimeArrayImp::RuntimeArrayImp(ExecState *exec, Bindings::Array *a) : ArrayInstanceImp (exec->lexicalInterpreter()->builtinArrayPrototype().imp(), a->getLength())
{
    // Always takes ownership of concrete array.
    _array = a;
}

RuntimeArrayImp::~RuntimeArrayImp()
{
    delete _array;
}


Value RuntimeArrayImp::get(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == lengthPropertyName)
        return Number(getLength());
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(&ok);
    if (ok) {
        if (index >= getLength())
            return Undefined();
        return getConcreteArray()->valueAt(exec, index);
    }
    
    return ObjectImp::get(exec, propertyName);
}

Value RuntimeArrayImp::get(ExecState *exec, unsigned index) const
{
    if (index >= getLength())
        return Undefined();
    return getConcreteArray()->valueAt(exec, index);
}

void RuntimeArrayImp::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
{
    if (propertyName == lengthPropertyName) {
        Object err = Error::create(exec,RangeError);
        exec->setException(err);
        return;
    }
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(&ok);
    if (ok) {
        getConcreteArray()->setValueAt(exec, index, value);
        return;
    }
    
    ObjectImp::put(exec, propertyName, value, attr);
}

void RuntimeArrayImp::put(ExecState *exec, unsigned index, const Value &value, int attr)
{
    if (index >= getLength()) {
        Object err = Error::create(exec,RangeError);
        exec->setException(err);
        return;
    }
    
    getConcreteArray()->setValueAt(exec, index, value);
}


bool RuntimeArrayImp::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == lengthPropertyName)
        return true;
    
    bool ok;
    unsigned index = propertyName.toArrayIndex(&ok);
    if (ok) {
        if (index >= getLength())
            return false;
        return true;
    }
    
    return ObjectImp::hasOwnProperty(exec, propertyName);
}

bool RuntimeArrayImp::hasOwnProperty(ExecState *exec, unsigned index) const
{
    if (index >= getLength())
        return false;
    return true;
}

bool RuntimeArrayImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    return false;
}

bool RuntimeArrayImp::deleteProperty(ExecState *exec, unsigned index)
{
    return false;
}
