/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

using namespace std;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(Arguments);

const ClassInfo Arguments::s_info = { "Arguments", &JSNonFinalObject::s_info, 0, 0, CREATE_METHOD_TABLE(Arguments) };

void Arguments::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    JSObject::visitChildren(thisObject, visitor);

    if (thisObject->d->registerArray)
        visitor.appendValues(thisObject->d->registerArray.get(), thisObject->d->numArguments);
    visitor.append(&thisObject->d->callee);
    if (thisObject->d->activation)
        visitor.append(&thisObject->d->activation);
}

void Arguments::destroy(JSCell* cell)
{
    jsCast<Arguments*>(cell)->Arguments::~Arguments();
}

void Arguments::copyToArguments(ExecState* exec, CallFrame* callFrame, uint32_t length)
{
    if (UNLIKELY(d->overrodeLength)) {
        length = min(get(exec, exec->propertyNames().length).toUInt32(exec), length);
        for (unsigned i = 0; i < length; i++)
            callFrame->setArgument(i, get(exec, i));
        return;
    }
    ASSERT(length == this->length(exec));
    for (size_t i = 0; i < length; ++i) {
        if (!d->deletedArguments || !d->deletedArguments[i])
            callFrame->setArgument(i, argument(i).get());
        else
            callFrame->setArgument(i, get(exec, i));
    }
}

void Arguments::fillArgList(ExecState* exec, MarkedArgumentBuffer& args)
{
    if (UNLIKELY(d->overrodeLength)) {
        unsigned length = get(exec, exec->propertyNames().length).toUInt32(exec); 
        for (unsigned i = 0; i < length; i++) 
            args.append(get(exec, i)); 
        return;
    }
    uint32_t length = this->length(exec);
    for (size_t i = 0; i < length; ++i) {
        if (!d->deletedArguments || !d->deletedArguments[i])
            args.append(argument(i).get());
        else
            args.append(get(exec, i));
    }
}

bool Arguments::getOwnPropertySlotByIndex(JSCell* cell, ExecState* exec, unsigned i, PropertySlot& slot)
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    if (i < thisObject->d->numArguments && (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])) {
        slot.setValue(thisObject->argument(i).get());
        return true;
    }

    return JSObject::getOwnPropertySlot(thisObject, exec, Identifier(exec, UString::number(i)), slot);
}
    
void Arguments::createStrictModeCallerIfNecessary(ExecState* exec)
{
    if (d->overrodeCaller)
        return;

    d->overrodeCaller = true;
    PropertyDescriptor descriptor;
    descriptor.setAccessorDescriptor(globalObject()->throwTypeErrorGetterSetter(exec), DontEnum | DontDelete | Accessor);
    methodTable()->defineOwnProperty(this, exec, exec->propertyNames().caller, descriptor, false);
}

void Arguments::createStrictModeCalleeIfNecessary(ExecState* exec)
{
    if (d->overrodeCallee)
        return;
    
    d->overrodeCallee = true;
    PropertyDescriptor descriptor;
    descriptor.setAccessorDescriptor(globalObject()->throwTypeErrorGetterSetter(exec), DontEnum | DontDelete | Accessor);
    methodTable()->defineOwnProperty(this, exec, exec->propertyNames().callee, descriptor, false);
}

bool Arguments::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex && i < thisObject->d->numArguments && (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])) {
        slot.setValue(thisObject->argument(i).get());
        return true;
    }

    if (propertyName == exec->propertyNames().length && LIKELY(!thisObject->d->overrodeLength)) {
        slot.setValue(jsNumber(thisObject->d->numArguments));
        return true;
    }

    if (propertyName == exec->propertyNames().callee && LIKELY(!thisObject->d->overrodeCallee)) {
        if (!thisObject->d->isStrictMode) {
            slot.setValue(thisObject->d->callee.get());
            return true;
        }
        thisObject->createStrictModeCalleeIfNecessary(exec);
    }

    if (propertyName == exec->propertyNames().caller && thisObject->d->isStrictMode)
        thisObject->createStrictModeCallerIfNecessary(exec);

    return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool Arguments::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    Arguments* thisObject = jsCast<Arguments*>(object);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex && i < thisObject->d->numArguments && (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])) {
        descriptor.setDescriptor(thisObject->argument(i).get(), None);
        return true;
    }
    
    if (propertyName == exec->propertyNames().length && LIKELY(!thisObject->d->overrodeLength)) {
        descriptor.setDescriptor(jsNumber(thisObject->d->numArguments), DontEnum);
        return true;
    }
    
    if (propertyName == exec->propertyNames().callee && LIKELY(!thisObject->d->overrodeCallee)) {
        if (!thisObject->d->isStrictMode) {
            descriptor.setDescriptor(thisObject->d->callee.get(), DontEnum);
            return true;
        }
        thisObject->createStrictModeCalleeIfNecessary(exec);
    }

    if (propertyName == exec->propertyNames().caller && thisObject->d->isStrictMode)
        thisObject->createStrictModeCallerIfNecessary(exec);
    
    return JSObject::getOwnPropertyDescriptor(thisObject, exec, propertyName, descriptor);
}

void Arguments::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    Arguments* thisObject = jsCast<Arguments*>(object);
    for (unsigned i = 0; i < thisObject->d->numArguments; ++i) {
        if (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])
            propertyNames.add(Identifier(exec, UString::number(i)));
    }
    if (mode == IncludeDontEnumProperties) {
        propertyNames.add(exec->propertyNames().callee);
        propertyNames.add(exec->propertyNames().length);
    }
    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

void Arguments::putByIndex(JSCell* cell, ExecState* exec, unsigned i, JSValue value)
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    if (i < static_cast<unsigned>(thisObject->d->numArguments) && (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])) {
        thisObject->argument(i).set(exec->globalData(), thisObject, value);
        return;
    }

    PutPropertySlot slot;
    JSObject::put(thisObject, exec, Identifier(exec, UString::number(i)), value, slot);
}

void Arguments::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex && i < thisObject->d->numArguments && (!thisObject->d->deletedArguments || !thisObject->d->deletedArguments[i])) {
        thisObject->argument(i).set(exec->globalData(), thisObject, value);
        return;
    }

    if (propertyName == exec->propertyNames().length && !thisObject->d->overrodeLength) {
        thisObject->d->overrodeLength = true;
        thisObject->putDirect(exec->globalData(), propertyName, value, DontEnum);
        return;
    }

    if (propertyName == exec->propertyNames().callee && !thisObject->d->overrodeCallee) {
        if (!thisObject->d->isStrictMode) {
            thisObject->d->overrodeCallee = true;
            thisObject->putDirect(exec->globalData(), propertyName, value, DontEnum);
            return;
        }
        thisObject->createStrictModeCalleeIfNecessary(exec);
    }

    if (propertyName == exec->propertyNames().caller && thisObject->d->isStrictMode)
        thisObject->createStrictModeCallerIfNecessary(exec);

    JSObject::put(thisObject, exec, propertyName, value, slot);
}

bool Arguments::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned i) 
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    if (i < thisObject->d->numArguments) {
        if (!thisObject->d->deletedArguments) {
            thisObject->d->deletedArguments = adoptArrayPtr(new bool[thisObject->d->numArguments]);
            memset(thisObject->d->deletedArguments.get(), 0, sizeof(bool) * thisObject->d->numArguments);
        }
        if (!thisObject->d->deletedArguments[i]) {
            thisObject->d->deletedArguments[i] = true;
            return true;
        }
    }

    return JSObject::deleteProperty(thisObject, exec, Identifier(exec, UString::number(i)));
}

bool Arguments::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName) 
{
    Arguments* thisObject = jsCast<Arguments*>(cell);
    bool isArrayIndex;
    unsigned i = propertyName.toArrayIndex(isArrayIndex);
    if (isArrayIndex && i < thisObject->d->numArguments) {
        if (!thisObject->d->deletedArguments) {
            thisObject->d->deletedArguments = adoptArrayPtr(new bool[thisObject->d->numArguments]);
            memset(thisObject->d->deletedArguments.get(), 0, sizeof(bool) * thisObject->d->numArguments);
        }
        if (!thisObject->d->deletedArguments[i]) {
            thisObject->d->deletedArguments[i] = true;
            return true;
        }
    }

    if (propertyName == exec->propertyNames().length && !thisObject->d->overrodeLength) {
        thisObject->d->overrodeLength = true;
        return true;
    }

    if (propertyName == exec->propertyNames().callee && !thisObject->d->overrodeCallee) {
        if (!thisObject->d->isStrictMode) {
            thisObject->d->overrodeCallee = true;
            return true;
        }
        thisObject->createStrictModeCalleeIfNecessary(exec);
    }
    
    if (propertyName == exec->propertyNames().caller && !thisObject->d->isStrictMode)
        thisObject->createStrictModeCallerIfNecessary(exec);

    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

void Arguments::tearOff(CallFrame* callFrame)
{
    if (isTornOff())
        return;

    if (!d->numArguments)
        return;

    d->registerArray = adoptArrayPtr(new WriteBarrier<Unknown>[d->numArguments]);
    d->registers = d->registerArray.get() + CallFrame::offsetFor(d->numArguments + 1);

    if (!callFrame->isInlineCallFrame()) {
        for (size_t i = 0; i < d->numArguments; ++i)
            argument(i).set(callFrame->globalData(), this, callFrame->argument(i));
        return;
    }

    InlineCallFrame* inlineCallFrame = callFrame->inlineCallFrame();
    for (size_t i = 0; i < d->numArguments; ++i) {
        ValueRecovery& recovery = inlineCallFrame->arguments[i + 1];
        // In the future we'll support displaced recoveries (indicating that the
        // argument was flushed to a different location), but for now we don't do
        // that so this code will fail if that were to happen. On the other hand,
        // it's much less likely that we'll support in-register recoveries since
        // this code does not (easily) have access to registers.
        JSValue value;
        Register* location = &callFrame->registers()[CallFrame::argumentOffset(i)];
        switch (recovery.technique()) {
        case AlreadyInRegisterFile:
            value = location->jsValue();
            break;
        case AlreadyInRegisterFileAsUnboxedInt32:
            value = jsNumber(location->unboxedInt32());
            break;
        case AlreadyInRegisterFileAsUnboxedCell:
            value = location->unboxedCell();
            break;
        case AlreadyInRegisterFileAsUnboxedBoolean:
            value = jsBoolean(location->unboxedBoolean());
            break;
        case AlreadyInRegisterFileAsUnboxedDouble:
#if USE(JSVALUE64)
            value = jsNumber(*bitwise_cast<double*>(location));
#else
            value = location->jsValue();
#endif
            break;
        case Constant:
            value = recovery.constant();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        argument(i).set(callFrame->globalData(), this, value);
    }
}

} // namespace JSC
