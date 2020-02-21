/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ClonedArguments.h"

#include "GetterSetter.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ClonedArguments);

const ClassInfo ClonedArguments::s_info = { "Arguments", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ClonedArguments) };

ClonedArguments::ClonedArguments(VM& vm, Structure* structure, Butterfly* butterfly)
    : Base(vm, structure, butterfly)
{
}

ClonedArguments* ClonedArguments::createEmpty(
    VM& vm, Structure* structure, JSFunction* callee, unsigned length)
{
    unsigned vectorLength = length;
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH)
        return 0;

    Butterfly* butterfly;
    if (UNLIKELY(structure->mayInterceptIndexedAccesses() || structure->storedPrototypeObject()->needsSlowPutIndexing(vm))) {
        butterfly = createArrayStorageButterfly(vm, nullptr, structure, length, vectorLength);
        butterfly->arrayStorage()->m_numValuesInVector = vectorLength;
    } else {
        IndexingHeader indexingHeader;
        indexingHeader.setVectorLength(vectorLength);
        indexingHeader.setPublicLength(length);
        butterfly = Butterfly::tryCreate(vm, 0, 0, structure->outOfLineCapacity(), true, indexingHeader, vectorLength * sizeof(EncodedJSValue));
        if (!butterfly)
            return 0;

        for (unsigned i = length; i < vectorLength; ++i)
            butterfly->contiguous().atUnsafe(i).clear();
    }
    
    ClonedArguments* result =
        new (NotNull, allocateCell<ClonedArguments>(vm.heap))
        ClonedArguments(vm, structure, butterfly);
    result->finishCreation(vm);

    result->m_callee.set(vm, result, callee);
    result->putDirect(vm, clonedArgumentsLengthPropertyOffset, jsNumber(length));
    return result;
}

ClonedArguments* ClonedArguments::createEmpty(JSGlobalObject* globalObject, JSFunction* callee, unsigned length)
{
    VM& vm = globalObject->vm();
    // NB. Some clients might expect that the global object of of this object is the global object
    // of the callee. We don't do this for now, but maybe we should.
    ClonedArguments* result = createEmpty(vm, globalObject->clonedArgumentsStructure(), callee, length);
    ASSERT(!result->needsSlowPutIndexing(vm) || shouldUseSlowPut(result->structure(vm)->indexingType()));
    return result;
}

ClonedArguments* ClonedArguments::createWithInlineFrame(JSGlobalObject* globalObject, CallFrame* targetFrame, InlineCallFrame* inlineCallFrame, ArgumentsMode mode)
{
    JSFunction* callee;
    
    if (inlineCallFrame)
        callee = jsCast<JSFunction*>(inlineCallFrame->calleeRecovery.recover(targetFrame));
    else
        callee = jsCast<JSFunction*>(targetFrame->jsCallee());

    ClonedArguments* result = nullptr;
    
    unsigned length = 0; // Initialize because VC needs it.
    switch (mode) {
    case ArgumentsMode::Cloned: {
        if (inlineCallFrame) {
            if (inlineCallFrame->argumentCountRegister.isValid())
                length = targetFrame->r(inlineCallFrame->argumentCountRegister).unboxedInt32();
            else
                length = inlineCallFrame->argumentCountIncludingThis;
            length--;
            result = createEmpty(globalObject, callee, length);

            for (unsigned i = length; i--;)
                result->putDirectIndex(globalObject, i, inlineCallFrame->argumentsWithFixup[i + 1].recover(targetFrame));
        } else {
            length = targetFrame->argumentCount();
            result = createEmpty(globalObject, callee, length);

            for (unsigned i = length; i--;)
                result->putDirectIndex(globalObject, i, targetFrame->uncheckedArgument(i));
        }
        break;
    }
        
    case ArgumentsMode::FakeValues: {
        result = createEmpty(globalObject, callee, 0);
        break;
    } }

    ASSERT(globalObject->clonedArgumentsStructure() == result->structure(globalObject->vm()));
    ASSERT(!result->needsSlowPutIndexing(globalObject->vm()) || shouldUseSlowPut(result->structure(globalObject->vm())->indexingType()));
    return result;
}

ClonedArguments* ClonedArguments::createWithMachineFrame(JSGlobalObject* globalObject, CallFrame* targetFrame, ArgumentsMode mode)
{
    ClonedArguments* result = createWithInlineFrame(globalObject, targetFrame, nullptr, mode);
    ASSERT(!result->needsSlowPutIndexing(globalObject->vm()) || shouldUseSlowPut(result->structure(globalObject->vm())->indexingType()));
    return result;
}

ClonedArguments* ClonedArguments::createByCopyingFrom(
    JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, unsigned length,
    JSFunction* callee)
{
    VM& vm = globalObject->vm();
    ClonedArguments* result = createEmpty(vm, structure, callee, length);
    
    for (unsigned i = length; i--;)
        result->putDirectIndex(globalObject, i, argumentStart[i].jsValue());
    ASSERT(!result->needsSlowPutIndexing(vm) || shouldUseSlowPut(result->structure(vm)->indexingType()));
    return result;
}

Structure* ClonedArguments::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, IndexingType indexingType)
{
    Structure* structure = Structure::create(vm, globalObject, prototype, TypeInfo(ClonedArgumentsType, StructureFlags), info(), indexingType);
    structure->addPropertyWithoutTransition(
        vm, vm.propertyNames->length, static_cast<unsigned>(PropertyAttribute::DontEnum),
        [&] (const GCSafeConcurrentJSLocker&, PropertyOffset offset, PropertyOffset newMaxOffset) {
            RELEASE_ASSERT(offset == clonedArgumentsLengthPropertyOffset);
            structure->setMaxOffset(vm, newMaxOffset);
        });
    return structure;
}

Structure* ClonedArguments::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    // We use contiguous storage because optimizations in the FTL assume that cloned arguments creation always produces the same initial structure.
    return createStructure(vm, globalObject, prototype, NonArrayWithContiguous);
}

Structure* ClonedArguments::createSlowPutStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return createStructure(vm, globalObject, prototype, NonArrayWithSlowPutArrayStorage);
}

bool ClonedArguments::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName ident, PropertySlot& slot)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(object);
    VM& vm = globalObject->vm();

    if (!thisObject->specialsMaterialized()) {
        FunctionExecutable* executable = jsCast<FunctionExecutable*>(thisObject->m_callee->executable());
        bool isStrictMode = executable->isStrictMode();

        if (ident == vm.propertyNames->callee) {
            if (isStrictMode) {
                slot.setGetterSlot(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::Accessor, thisObject->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter());
                return true;
            }
            slot.setValue(thisObject, 0, thisObject->m_callee.get());
            return true;
        }

        if (ident == vm.propertyNames->iteratorSymbol) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum), thisObject->globalObject(vm)->arrayProtoValuesFunction());
            return true;
        }
    }

    return Base::getOwnPropertySlot(thisObject, globalObject, ident, slot);
}

void ClonedArguments::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& array, EnumerationMode mode)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(object);
    thisObject->materializeSpecialsIfNecessary(globalObject);
    Base::getOwnPropertyNames(thisObject, globalObject, array, mode);
}

bool ClonedArguments::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName ident, JSValue value, PutPropertySlot& slot)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(cell);
    VM& vm = globalObject->vm();
    
    if (ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol) {
        thisObject->materializeSpecialsIfNecessary(globalObject);
        PutPropertySlot dummy = slot; // Shadow the given PutPropertySlot to prevent caching.
        return Base::put(thisObject, globalObject, ident, value, dummy);
    }
    
    return Base::put(thisObject, globalObject, ident, value, slot);
}

bool ClonedArguments::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName ident)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(cell);
    VM& vm = globalObject->vm();
    
    if (ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol)
        thisObject->materializeSpecialsIfNecessary(globalObject);
    
    return Base::deleteProperty(thisObject, globalObject, ident);
}

bool ClonedArguments::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName ident, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(object);
    VM& vm = globalObject->vm();
    
    if (ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol)
        thisObject->materializeSpecialsIfNecessary(globalObject);
    
    return Base::defineOwnProperty(object, globalObject, ident, descriptor, shouldThrow);
}

void ClonedArguments::materializeSpecials(JSGlobalObject* globalObject)
{
    RELEASE_ASSERT(!specialsMaterialized());
    VM& vm = globalObject->vm();
    
    FunctionExecutable* executable = jsCast<FunctionExecutable*>(m_callee->executable());
    bool isStrictMode = executable->isStrictMode();
    
    if (isStrictMode)
        putDirectAccessor(globalObject, vm.propertyNames->callee, this->globalObject(vm)->throwTypeErrorArgumentsCalleeAndCallerGetterSetter(), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    else
        putDirect(vm, vm.propertyNames->callee, JSValue(m_callee.get()));

    putDirect(vm, vm.propertyNames->iteratorSymbol, this->globalObject(vm)->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    m_callee.clear();
}

void ClonedArguments::materializeSpecialsIfNecessary(JSGlobalObject* globalObject)
{
    if (!specialsMaterialized())
        materializeSpecials(globalObject);
}

void ClonedArguments::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_callee);
}

} // namespace JSC

