/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSString.h"

#include "JSGlobalObject.h"
#include "JSObject.h"
#include "StringObject.h"
#include "StringPrototype.h"

namespace JSC {

JSString::Rope::~Rope()
{
    for (unsigned i = 0; i < m_ropeLength; ++i) {
        Fiber& fiber = m_fibers[i];
        if (fiber.isRope())
            fiber.rope()->deref();
        else
            fiber.string()->deref();
    }
}

#define ROPE_COPY_CHARS_INLINE_CUTOFF 20

static inline void copyChars(UChar* destination, const UChar* source, unsigned numCharacters)
{
#ifdef ROPE_COPY_CHARS_INLINE_CUTOFF
    if (numCharacters <= ROPE_COPY_CHARS_INLINE_CUTOFF) {
        for (unsigned i = 0; i < numCharacters; ++i)
            destination[i] = source[i];
        return;
    }
#endif
    memcpy(destination, source, numCharacters * sizeof(UChar));
}

// Overview: this methods converts a JSString from holding a string in rope form
// down to a simple UString representation.  It does so by building up the string
// backwards, since we want to avoid recursion, we expect that the tree structure
// representing the rope is likely imbalanced with more nodes down the left side
// (since appending to the string is likely more common) - and as such resolving
// in this fashion should minimize work queue size.  (If we built the queue forwards
// we would likely have to place all of the constituent UString::Reps into the
// Vector before performing any concatenation, but by working backwards we likely
// only fill the queue with the number of substrings at any given level in a
// rope-of-ropes.)
void JSString::resolveRope() const
{
    ASSERT(isRope());

    // Allocate the buffer to hold the final string, position initially points to the end.
    UChar* buffer = static_cast<UChar*>(fastMalloc(m_length * sizeof(UChar)));
    UChar* position = buffer + m_length;

    // Start with the current Rope.
    Vector<Rope::Fiber, 32> workQueue;
    Rope* rope = m_rope.get();
    while (true) {
        // Copy the contents of the current rope into the workQueue, with the last item in 'currentFiber'
        // (we will be working backwards over the rope).
        unsigned ropeLengthMinusOne = rope->ropeLength() - 1;
        for (unsigned i = 0; i < ropeLengthMinusOne; ++i)
            workQueue.append(rope->fibers(i));
        Rope::Fiber currentFiber = rope->fibers(ropeLengthMinusOne);

        // Spin backwards over the workQueue (starting with currentFiber),
        // writing the strings into the buffer.
        while (currentFiber.isString()) {
            UString::Rep* string = currentFiber.string();
            unsigned length = string->len;
            position -= length;
            copyChars(position, string->data(), length);

            // Was this the last item in the work queue?
            if (workQueue.isEmpty())
                goto breakOutOfTwoLoops;
            // No! - set the next item up to process.
            currentFiber = workQueue.last();
            workQueue.removeLast();
        }

        // If we get here we fell out of the loop concatenating strings - currentFiber is a rope.
        // set the 'rope' variable, and continue around the loop.
        ASSERT(currentFiber.isRope());
        rope = currentFiber.rope();
    }
breakOutOfTwoLoops:

    // Create a string from the UChar buffer, clear the rope RefPtr.
    ASSERT(buffer == position);
    m_value = UString::Rep::create(buffer, m_length, false);
    m_rope.clear();

    ASSERT(!isRope());
}

JSValue JSString::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSString*>(this);
}

bool JSString::getPrimitiveNumber(ExecState*, double& number, JSValue& result)
{
    result = this;
    number = value().toDouble();
    return false;
}

bool JSString::toBoolean(ExecState*) const
{
    return m_length;
}

double JSString::toNumber(ExecState*) const
{
    return value().toDouble();
}

UString JSString::toString(ExecState*) const
{
    return value();
}

UString JSString::toThisString(ExecState*) const
{
    return value();
}

JSString* JSString::toThisJSString(ExecState*)
{
    return this;
}

inline StringObject* StringObject::create(ExecState* exec, JSString* string)
{
    return new (exec) StringObject(exec->lexicalGlobalObject()->stringObjectStructure(), string);
}

JSObject* JSString::toObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

JSObject* JSString::toThisObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

bool JSString::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(exec, propertyName, slot))
        return true;
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValue(exec->lexicalGlobalObject()->stringPrototype());
        return true;
    }
    slot.setBase(this);
    JSObject* object;
    for (JSValue prototype = exec->lexicalGlobalObject()->stringPrototype(); !prototype.isNull(); prototype = object->prototype()) {
        object = asObject(prototype);
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;
    }
    slot.setUndefined();
    return true;
}

bool JSString::getStringPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    if (propertyName == exec->propertyNames().length) {
        descriptor.setDescriptor(jsNumber(exec, m_length), DontEnum | DontDelete | ReadOnly);
        return true;
    }
    
    bool isStrictUInt32;
    unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
    if (isStrictUInt32 && i < m_length) {
        descriptor.setDescriptor(jsSingleCharacterSubstring(exec, value(), i), DontDelete | ReadOnly);
        return true;
    }
    
    return false;
}

bool JSString::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    if (getStringPropertyDescriptor(exec, propertyName, descriptor))
        return true;
    if (propertyName != exec->propertyNames().underscoreProto)
        return false;
    descriptor.setDescriptor(exec->lexicalGlobalObject()->stringPrototype(), DontEnum);
    return true;
}

bool JSString::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(exec, propertyName, slot))
        return true;
    return JSString::getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

} // namespace JSC
