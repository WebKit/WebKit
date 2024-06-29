/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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

#include "JSGlobalObjectFunctions.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "StringObject.h"
#include "StrongInlines.h"
#include "StructureInlines.h"

namespace JSC {
    
const ClassInfo JSString::s_info = { "string"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSString) };

Structure* JSString::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, defaultTypeInfo(), info());
}

JSString* JSString::createEmptyString(VM& vm)
{
    JSString* newString = new (NotNull, allocateCell<JSString>(vm)) JSString(vm, *StringImpl::empty());
    newString->finishCreation(vm);
    return newString;
}

template<>
void JSRopeString::RopeBuilder<RecordOverflow>::expand()
{
    RELEASE_ASSERT(!this->hasOverflowed());
    ASSERT(m_strings.size() == JSRopeString::s_maxInternalRopeLength);
    static_assert(3 == JSRopeString::s_maxInternalRopeLength);
    ASSERT(m_length);
    ASSERT(asString(m_strings.at(0))->length());
    ASSERT(asString(m_strings.at(1))->length());
    ASSERT(asString(m_strings.at(2))->length());

    JSString* string = JSRopeString::create(m_vm, asString(m_strings.at(0)), asString(m_strings.at(1)), asString(m_strings.at(2)));
    ASSERT(string->length() == m_length);
    m_strings.clear();
    m_strings.append(string);
}

void JSString::dumpToStream(const JSCell* cell, PrintStream& out)
{
    const JSString* thisObject = jsCast<const JSString*>(cell);
    out.printf("<%p, %s, [%u], ", thisObject, thisObject->className().characters(), thisObject->length());
    uintptr_t pointer = thisObject->fiberConcurrently();
    if (pointer & isRopeInPointer) {
        if (pointer & JSRopeString::isSubstringInPointer)
            out.printf("[substring]");
        else
            out.printf("[rope]");
    } else {
        if (WTF::StringImpl* ourImpl = bitwise_cast<StringImpl*>(pointer)) {
            if (ourImpl->is8Bit())
                out.printf("[8 %p]", ourImpl->span8().data());
            else
                out.printf("[16 %p]", ourImpl->span16().data());
        }
    }
    out.printf(">");
}

bool JSString::equalSlowCase(JSGlobalObject* globalObject, JSString* other) const
{
    return equalInline(globalObject, other);
}

size_t JSString::estimatedSize(JSCell* cell, VM& vm)
{
    JSString* thisObject = asString(cell);
    uintptr_t pointer = thisObject->fiberConcurrently();
    if (pointer & isRopeInPointer)
        return Base::estimatedSize(cell, vm);
    return Base::estimatedSize(cell, vm) + bitwise_cast<StringImpl*>(pointer)->costDuringGC();
}

template<typename Visitor>
void JSString::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSString* thisObject = asString(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    
    uintptr_t pointer = thisObject->fiberConcurrently();
    if (pointer & isRopeInPointer) {
        if (pointer & JSRopeString::isSubstringInPointer) {
            visitor.appendUnbarriered(static_cast<JSRopeString*>(thisObject)->fiber1());
            return;
        }
        for (unsigned index = 0; index < JSRopeString::s_maxInternalRopeLength; ++index) {
            JSString* fiber = nullptr;
            switch (index) {
            case 0:
                fiber = bitwise_cast<JSString*>(pointer & JSRopeString::stringMask);
                break;
            case 1:
                fiber = static_cast<JSRopeString*>(thisObject)->fiber1();
                break;
            case 2:
                fiber = static_cast<JSRopeString*>(thisObject)->fiber2();
                break;
            default:
                ASSERT_NOT_REACHED();
                return;
            }
            if (!fiber)
                break;
            visitor.appendUnbarriered(fiber);
        }
        return;
    }
    if (StringImpl* impl = bitwise_cast<StringImpl*>(pointer))
        visitor.reportExtraMemoryVisited(impl->costDuringGC());
}

DEFINE_VISIT_CHILDREN(JSString);

template<typename CharacterType>
void JSRopeString::resolveRopeInternalNoSubstring(CharacterType* buffer, uint8_t* stackLimit) const
{
    resolveToBuffer(fiber0(), fiber1(), fiber2(), buffer, length(), stackLimit);
}

void JSRopeString::iterRopeInternalNoSubstring(jsstring_iterator* iter) const
{
    for (size_t i = 0; i < s_maxInternalRopeLength && fiber(i) && !iter->stop; ++i) {
        if (fiber(i)->isRope()) {
            iterRopeSlowCase(iter);
            return;
        }
    }

    size_t position = 0;

    for (size_t i = 0; i < s_maxInternalRopeLength && fiber(i) && !iter->stop; ++i) {
        const StringImpl& fiberString = *fiber(i)->valueInternal().impl();
        unsigned length = fiberString.length();
        if (fiberString.is8Bit())
            StringImpl::iterCharacters(iter, position, fiberString.span8().data(), length);
        else
            StringImpl::iterCharacters(iter, position, fiberString.span16().data(), length);
        position += length;
    }

    ASSERT(iter->stop || length() == position);
}

bool JSString::equalSlowCase(JSGlobalObject* globalObject, const char* ptr, size_t len) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (length() != len)
        return false;

    String str1 = value(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    return WTF::equal(*str1.impl(), StringView({ ptr, len }));
}

AtomString JSRopeString::resolveRopeToAtomString(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto convertToAtomString = [](const String& string) -> AtomString {
        ASSERT(!string.impl() || string.impl()->isAtom());
        return static_cast<AtomStringImpl*>(string.impl());
    };

    if (length() > maxLengthForOnStackResolve) {
        scope.release();
        constexpr bool reportAllocation = true;
        return convertToAtomString(resolveRopeWithFunction<reportAllocation>(globalObject, [&] (Ref<StringImpl>&& newImpl) {
            return AtomStringImpl::add(newImpl.ptr());
        }));
    }

    AtomString atomString;
    uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
    if (!isSubstring()) {
        if (is8Bit()) {
            LChar buffer[maxLengthForOnStackResolve];
            resolveRopeInternalNoSubstring(buffer, stackLimit);
            atomString = std::span<const LChar> { buffer, length() };
        } else {
            UChar buffer[maxLengthForOnStackResolve];
            resolveRopeInternalNoSubstring(buffer, stackLimit);
            atomString = std::span<const UChar> { buffer, length() };
        }
    } else
        atomString = StringView { substringBase()->valueInternal() }.substring(substringOffset(), length()).toAtomString();

    size_t sizeToReport = atomString.impl()->hasOneRef() ? atomString.impl()->cost() : 0;
    convertToNonRope(String { atomString });
    // If we resolved a string that didn't previously exist, notify the heap that we've grown.
    vm.heap.reportExtraMemoryAllocated(this, sizeToReport);
    return atomString;
}

RefPtr<AtomStringImpl> JSRopeString::resolveRopeToExistingAtomString(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (length() > maxLengthForOnStackResolve) {
        RefPtr<AtomStringImpl> existingAtomString;
        constexpr bool reportAllocation = true;
        resolveRopeWithFunction<reportAllocation>(globalObject, [&] (Ref<StringImpl>&& newImpl) -> Ref<StringImpl> {
            existingAtomString = AtomStringImpl::lookUp(newImpl.ptr());
            if (existingAtomString)
                return Ref { *existingAtomString };
            return WTFMove(newImpl);
        });
        RETURN_IF_EXCEPTION(scope, nullptr);
        return existingAtomString;
    }
    
    RefPtr<AtomStringImpl> existingAtomString;
    if (!isSubstring()) {
        uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
        if (is8Bit()) {
            LChar buffer[maxLengthForOnStackResolve];
            resolveRopeInternalNoSubstring(buffer, stackLimit);
            existingAtomString = AtomStringImpl::lookUp(std::span { buffer, length() });
        } else {
            UChar buffer[maxLengthForOnStackResolve];
            resolveRopeInternalNoSubstring(buffer, stackLimit);
            existingAtomString = AtomStringImpl::lookUp(std::span { buffer, length() });
        }
    } else
        existingAtomString = StringView { substringBase()->valueInternal() }.substring(substringOffset(), length()).toExistingAtomString().releaseImpl();

    if (existingAtomString)
        convertToNonRope(*existingAtomString);
    return existingAtomString;
}

template<bool reportAllocation, typename Function>
const String& JSRopeString::resolveRopeWithFunction(JSGlobalObject* nullOrGlobalObjectForOOM, Function&& function) const
{
    ASSERT(isRope());
    
    VM& vm = this->vm();
    if (isSubstring()) {
        ASSERT(!substringBase()->isRope());
        auto newImpl = substringBase()->valueInternal().substringSharingImpl(substringOffset(), length());
        convertToNonRope(function(newImpl.releaseImpl().releaseNonNull()));
        return valueInternal();
    }
    
    if (is8Bit()) {
        LChar* buffer;
        auto newImpl = StringImpl::tryCreateUninitialized(length(), buffer);
        if (!newImpl) {
            outOfMemory(nullOrGlobalObjectForOOM);
            return nullString();
        }

        size_t sizeToReport = newImpl->cost();
        uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
        resolveRopeInternalNoSubstring(buffer, stackLimit);
        convertToNonRope(function(newImpl.releaseNonNull()));
        if constexpr (reportAllocation)
            vm.heap.reportExtraMemoryAllocated(this, sizeToReport);
        return valueInternal();
    }
    
    UChar* buffer;
    auto newImpl = StringImpl::tryCreateUninitialized(length(), buffer);
    if (!newImpl) {
        outOfMemory(nullOrGlobalObjectForOOM);
        return nullString();
    }
    
    size_t sizeToReport = newImpl->cost();
    uint8_t* stackLimit = bitwise_cast<uint8_t*>(vm.softStackLimit());
    resolveRopeInternalNoSubstring(buffer, stackLimit);
    convertToNonRope(function(newImpl.releaseNonNull()));
    if constexpr (reportAllocation)
        vm.heap.reportExtraMemoryAllocated(this, sizeToReport);
    return valueInternal();
}

const String& JSRopeString::resolveRope(JSGlobalObject* nullOrGlobalObjectForOOM) const
{
    constexpr bool reportAllocation = true;
    return resolveRopeWithFunction<reportAllocation>(nullOrGlobalObjectForOOM, [] (Ref<StringImpl>&& newImpl) {
        return WTFMove(newImpl);
    });
}

const String& JSRopeString::resolveRopeWithoutGC() const
{
    constexpr bool reportAllocation = false;
    return resolveRopeWithFunction<reportAllocation>(nullptr, [] (Ref<StringImpl>&& newImpl) {
        return WTFMove(newImpl);
    });
}


void JSRopeString::iterRope(jsstring_iterator *iter) const
{
     ASSERT(isRope());

    if (isSubstring()) {
        ASSERT(!substringBase()->isRope());
        StringImpl *impl = substringBase()->valueInternal().impl();

        if (impl->is8Bit()) {
            auto ptr = impl->span8().data() + substringOffset();
            size_t end = length();
            iter->append8(iter, (void*)ptr, end);
        } else {
            auto ptr = impl->span16().data() + substringOffset();
            size_t end = length();
            iter->append16(iter, (void*)ptr, end);
        }

        return;
    }


    iterRopeInternalNoSubstring(iter);
}

void JSRopeString::iterRopeSlowCase(jsstring_iterator* iter) const
{
    size_t position = length(); // We will be working backwards over the rope.
    Vector<JSString*, 32, UnsafeVectorOverflow> workQueue; // These strings are kept alive by the parent rope, so using a Vector is OK.

    for (size_t i = 0; i < s_maxInternalRopeLength && fiber(i); ++i)
        workQueue.append(fiber(i));

    while (!workQueue.isEmpty() && !iter->stop) {
        JSString* currentFiber = workQueue.last();
        workQueue.removeLast();

        if (currentFiber->isRope()) {
            JSRopeString* currentFiberAsRope = static_cast<JSRopeString*>(currentFiber);
            if (currentFiberAsRope->isSubstring()) {
                ASSERT(!currentFiberAsRope->substringBase()->isRope());
                StringImpl* string = static_cast<StringImpl*>(
                    currentFiberAsRope->substringBase()->valueInternal().impl());
                unsigned offset = currentFiberAsRope->substringOffset();
                unsigned length = currentFiberAsRope->length();
                position -= length;
                if (string->is8Bit())
                    StringImpl::iterCharacters(iter, position, string->span8().data() + offset, length);
                else
                    StringImpl::iterCharacters(iter, position, string->span16().data() + offset, length);
                continue;
            }
            for (size_t i = 0; i < s_maxInternalRopeLength && currentFiberAsRope->fiber(i) && !iter->stop; ++i)
                workQueue.append(currentFiberAsRope->fiber(i));
            continue;
        }

        StringImpl* string = static_cast<StringImpl*>(currentFiber->valueInternal().impl());
        unsigned length = string->length();
        position -= length;
        if (string->is8Bit())
            StringImpl::iterCharacters(iter, position, string->span8().data(), length);
        else
            StringImpl::iterCharacters(iter, position, string->span16().data(), length);
    }

    ASSERT(position == 0 || iter->stop);
}

void JSRopeString::outOfMemory(JSGlobalObject* nullOrGlobalObjectForOOM) const
{
    ASSERT(isRope());
    if (nullOrGlobalObjectForOOM) {
        VM& vm = nullOrGlobalObjectForOOM->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
    }
}

JSValue JSString::toPrimitive(JSGlobalObject*, PreferredPrimitiveType) const
{
    return const_cast<JSString*>(this);
}

double JSString::toNumber(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto view = this->view(globalObject);
    RETURN_IF_EXCEPTION(scope, 0);
    return jsToNumber(view);
}

inline StringObject* StringObject::create(VM& vm, JSGlobalObject* globalObject, JSString* string)
{
    StringObject* object = new (NotNull, allocateCell<StringObject>(vm)) StringObject(vm, globalObject->stringObjectStructure());
    object->finishCreation(vm, string);
    return object;
}

JSObject* JSString::toObject(JSGlobalObject* globalObject) const
{
    return StringObject::create(globalObject->vm(), globalObject, const_cast<JSString*>(this));
}

bool JSString::getStringPropertyDescriptor(JSGlobalObject* globalObject, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->length) {
        descriptor.setDescriptor(jsNumber(length()), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        return true;
    }
    
    std::optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < length()) {
        descriptor.setDescriptor(getIndex(globalObject, index.value()), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        return true;
    }
    
    return false;
}

GCOwnedDataScope<const String&> JSString::tryGetValueWithoutGC() const
{
    if (isRope()) {
        // Pass nullptr for the JSGlobalObject so that resolveRope does not throw in the event of an OOM error.
        return { this, static_cast<const JSRopeString*>(this)->resolveRopeWithoutGC() };
    }
    return { this, valueInternal() };
}

JSString* jsStringWithCacheSlowCase(VM& vm, StringImpl& stringImpl)
{
    ASSERT(stringImpl.length() > 1 || (stringImpl.length() == 1 && stringImpl[0] > maxSingleCharacterString));
    JSString* string = JSString::create(vm, stringImpl);
    vm.lastCachedString.setWithoutWriteBarrier(string);
    return string;
}

} // namespace JSC
