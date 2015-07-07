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
#include "JSGlobalObjectFunctions.h"
#include "JSObject.h"
#include "JSCInlines.h"
#include "StringObject.h"
#include "StringPrototype.h"
#include "StrongInlines.h"

namespace JSC {
    
const ClassInfo JSString::s_info = { "string", 0, 0, 0, CREATE_METHOD_TABLE(JSString) };

void JSRopeString::RopeBuilder::expand()
{
    ASSERT(m_index == JSRopeString::s_maxInternalRopeLength);
    JSString* jsString = m_jsString;
    RELEASE_ASSERT(jsString);
    m_jsString = jsStringBuilder(&m_vm);
    m_index = 0;
    append(jsString);
}

void JSString::destroy(JSCell* cell)
{
    JSString* thisObject = static_cast<JSString*>(cell);
    thisObject->JSString::~JSString();
}

void JSString::dumpToStream(const JSCell* cell, PrintStream& out)
{
    const JSString* thisObject = jsCast<const JSString*>(cell);
    out.printf("<%p, %s, [%u], ", thisObject, thisObject->className(), thisObject->length());
    if (thisObject->isRope())
        out.printf("[rope]");
    else {
        WTF::StringImpl* ourImpl = thisObject->m_value.impl();
        if (ourImpl->is8Bit())
            out.printf("[8 %p]", ourImpl->characters8());
        else
            out.printf("[16 %p]", ourImpl->characters16());
    }
    out.printf(">");
}

void JSString::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSString* thisObject = jsCast<JSString*>(cell);
    Base::visitChildren(thisObject, visitor);
    
    if (thisObject->isRope())
        static_cast<JSRopeString*>(thisObject)->visitFibers(visitor);
    else {
        StringImpl* impl = thisObject->m_value.impl();
        ASSERT(impl);
        visitor.reportExtraMemoryUsage(thisObject, impl->costDuringGC());
    }
}

void JSRopeString::visitFibers(SlotVisitor& visitor)
{
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i)
        visitor.append(&m_fibers[i]);
}

static const unsigned maxLengthForOnStackResolve = 2048;

void JSRopeString::resolveRopeInternal8(LChar* buffer) const
{
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i) {
        if (m_fibers[i]->isRope()) {
            resolveRopeSlowCase8(buffer);
            return;
        }
    }

    LChar* position = buffer;
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i) {
        const StringImpl& fiberString = *m_fibers[i]->m_value.impl();
        unsigned length = fiberString.length();
        StringImpl::copyChars(position, fiberString.characters8(), length);
        position += length;
    }
    ASSERT((buffer + m_length) == position);
}

void JSRopeString::resolveRopeInternal16(UChar* buffer) const
{
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i) {
        if (m_fibers[i]->isRope()) {
            resolveRopeSlowCase(buffer);
            return;
        }
    }

    UChar* position = buffer;
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i) {
        const StringImpl& fiberString = *m_fibers[i]->m_value.impl();
        unsigned length = fiberString.length();
        if (fiberString.is8Bit())
            StringImpl::copyChars(position, fiberString.characters8(), length);
        else
            StringImpl::copyChars(position, fiberString.characters16(), length);
        position += length;
    }
    ASSERT((buffer + m_length) == position);
}

void JSRopeString::resolveRopeToAtomicString(ExecState* exec) const
{
    if (m_length > maxLengthForOnStackResolve) {
        resolveRope(exec);
        m_value = AtomicString(m_value);
        setIs8Bit(m_value.impl()->is8Bit());
        return;
    }

    if (is8Bit()) {
        LChar buffer[maxLengthForOnStackResolve];
        resolveRopeInternal8(buffer);
        m_value = AtomicString(buffer, m_length);
        setIs8Bit(m_value.impl()->is8Bit());
    } else {
        UChar buffer[maxLengthForOnStackResolve];
        resolveRopeInternal16(buffer);
        m_value = AtomicString(buffer, m_length);
        setIs8Bit(m_value.impl()->is8Bit());
    }

    clearFibers();

    // If we resolved a string that didn't previously exist, notify the heap that we've grown.
    if (m_value.impl()->hasOneRef())
        Heap::heap(this)->reportExtraMemoryCost(m_value.impl()->cost());
}

void JSRopeString::clearFibers() const
{
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i)
        m_fibers[i].clear();
}

AtomicStringImpl* JSRopeString::resolveRopeToExistingAtomicString(ExecState* exec) const
{
    if (m_length > maxLengthForOnStackResolve) {
        resolveRope(exec);
        if (AtomicStringImpl* existingAtomicString = AtomicString::find(m_value.impl())) {
            m_value = *existingAtomicString;
            setIs8Bit(m_value.impl()->is8Bit());
            clearFibers();
            return existingAtomicString;
        }
        return nullptr;
    }

    if (is8Bit()) {
        LChar buffer[maxLengthForOnStackResolve];
        resolveRopeInternal8(buffer);
        if (AtomicStringImpl* existingAtomicString = AtomicString::find(buffer, m_length)) {
            m_value = *existingAtomicString;
            setIs8Bit(m_value.impl()->is8Bit());
            clearFibers();
            return existingAtomicString;
        }
    } else {
        UChar buffer[maxLengthForOnStackResolve];
        resolveRopeInternal16(buffer);
        if (AtomicStringImpl* existingAtomicString = AtomicString::find(buffer, m_length)) {
            m_value = *existingAtomicString;
            setIs8Bit(m_value.impl()->is8Bit());
            clearFibers();
            return existingAtomicString;
        }
    }

    return nullptr;
}

void JSRopeString::resolveRope(ExecState* exec) const
{
    ASSERT(isRope());

    if (is8Bit()) {
        LChar* buffer;
        if (RefPtr<StringImpl> newImpl = StringImpl::tryCreateUninitialized(m_length, buffer)) {
            Heap::heap(this)->reportExtraMemoryCost(newImpl->cost());
            m_value = newImpl.release();
        } else {
            outOfMemory(exec);
            return;
        }
        resolveRopeInternal8(buffer);
        clearFibers();
        ASSERT(!isRope());
        return;
    }

    UChar* buffer;
    if (RefPtr<StringImpl> newImpl = StringImpl::tryCreateUninitialized(m_length, buffer)) {
        Heap::heap(this)->reportExtraMemoryCost(newImpl->cost());
        m_value = newImpl.release();
    } else {
        outOfMemory(exec);
        return;
    }

    resolveRopeInternal16(buffer);
    clearFibers();
    ASSERT(!isRope());
}

// Overview: These functions convert a JSString from holding a string in rope form
// down to a simple String representation. It does so by building up the string
// backwards, since we want to avoid recursion, we expect that the tree structure
// representing the rope is likely imbalanced with more nodes down the left side
// (since appending to the string is likely more common) - and as such resolving
// in this fashion should minimize work queue size.  (If we built the queue forwards
// we would likely have to place all of the constituent StringImpls into the
// Vector before performing any concatenation, but by working backwards we likely
// only fill the queue with the number of substrings at any given level in a
// rope-of-ropes.)    
void JSRopeString::resolveRopeSlowCase8(LChar* buffer) const
{
    LChar* position = buffer + m_length; // We will be working backwards over the rope.
    Vector<JSString*, 32, UnsafeVectorOverflow> workQueue; // Putting strings into a Vector is only OK because there are no GC points in this method.
    
    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i)
        workQueue.append(m_fibers[i].get());

    while (!workQueue.isEmpty()) {
        JSString* currentFiber = workQueue.last();
        workQueue.removeLast();

        if (currentFiber->isRope()) {
            JSRopeString* currentFiberAsRope = static_cast<JSRopeString*>(currentFiber);
            for (size_t i = 0; i < s_maxInternalRopeLength && currentFiberAsRope->m_fibers[i]; ++i)
                workQueue.append(currentFiberAsRope->m_fibers[i].get());
            continue;
        }

        StringImpl* string = static_cast<StringImpl*>(currentFiber->m_value.impl());
        unsigned length = string->length();
        position -= length;
        StringImpl::copyChars(position, string->characters8(), length);
    }

    ASSERT(buffer == position);
}

void JSRopeString::resolveRopeSlowCase(UChar* buffer) const
{
    UChar* position = buffer + m_length; // We will be working backwards over the rope.
    Vector<JSString*, 32, UnsafeVectorOverflow> workQueue; // These strings are kept alive by the parent rope, so using a Vector is OK.

    for (size_t i = 0; i < s_maxInternalRopeLength && m_fibers[i]; ++i)
        workQueue.append(m_fibers[i].get());

    while (!workQueue.isEmpty()) {
        JSString* currentFiber = workQueue.last();
        workQueue.removeLast();

        if (currentFiber->isRope()) {
            JSRopeString* currentFiberAsRope = static_cast<JSRopeString*>(currentFiber);
            for (size_t i = 0; i < s_maxInternalRopeLength && currentFiberAsRope->m_fibers[i]; ++i)
                workQueue.append(currentFiberAsRope->m_fibers[i].get());
            continue;
        }

        StringImpl* string = static_cast<StringImpl*>(currentFiber->m_value.impl());
        unsigned length = string->length();
        position -= length;
        if (string->is8Bit())
            StringImpl::copyChars(position, string->characters8(), length);
        else
            StringImpl::copyChars(position, string->characters16(), length);
    }

    ASSERT(buffer == position);
}

void JSRopeString::outOfMemory(ExecState* exec) const
{
    clearFibers();
    ASSERT(isRope());
    ASSERT(m_value.isNull());
    if (exec)
        throwOutOfMemoryError(exec);
}

JSString* JSRopeString::getIndexSlowCase(ExecState* exec, unsigned i)
{
    ASSERT(isRope());
    resolveRope(exec);
    // Return a safe no-value result, this should never be used, since the excetion will be thrown.
    if (exec->exception())
        return jsEmptyString(exec);
    ASSERT(!isRope());
    RELEASE_ASSERT(i < m_value.length());
    return jsSingleCharacterSubstring(exec, m_value, i);
}

JSValue JSString::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSString*>(this);
}

bool JSString::getPrimitiveNumber(ExecState* exec, double& number, JSValue& result) const
{
    result = this;
    number = jsToNumber(value(exec));
    return false;
}

bool JSString::toBoolean() const
{
    return m_length;
}

double JSString::toNumber(ExecState* exec) const
{
    return jsToNumber(value(exec));
}

inline StringObject* StringObject::create(VM& vm, JSGlobalObject* globalObject, JSString* string)
{
    StringObject* object = new (NotNull, allocateCell<StringObject>(vm.heap)) StringObject(vm, globalObject->stringObjectStructure());
    object->finishCreation(vm, string);
    return object;
}

JSObject* JSString::toObject(ExecState* exec, JSGlobalObject* globalObject) const
{
    return StringObject::create(exec->vm(), globalObject, const_cast<JSString*>(this));
}

JSValue JSString::toThis(JSCell* cell, ExecState* exec, ECMAMode ecmaMode)
{
    if (ecmaMode == StrictMode)
        return cell;
    return StringObject::create(exec->vm(), exec->lexicalGlobalObject(), jsCast<JSString*>(cell));
}

bool JSString::getStringPropertyDescriptor(ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    if (propertyName == exec->propertyNames().length) {
        descriptor.setDescriptor(jsNumber(m_length), DontEnum | DontDelete | ReadOnly);
        return true;
    }
    
    unsigned i = propertyName.asIndex();
    if (i < m_length) {
        ASSERT(i != PropertyName::NotAnIndex); // No need for an explicit check, the above test would always fail!
        descriptor.setDescriptor(getIndex(exec, i), DontDelete | ReadOnly);
        return true;
    }
    
    return false;
}

JSString* jsStringWithCacheSlowCase(VM& vm, StringImpl& stringImpl)
{
    auto addResult = vm.stringCache.add(&stringImpl, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = jsString(&vm, String(stringImpl));
    vm.lastCachedString.set(vm, addResult.iterator->value.get());
    return addResult.iterator->value.get();
}

} // namespace JSC
