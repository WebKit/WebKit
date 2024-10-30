/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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

#include "InlineCallFrame.h"
#include "JSCInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ClonedArguments);

const ClassInfo ClonedArguments::s_info = { "Arguments"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ClonedArguments) };

ClonedArguments::ClonedArguments(VM& vm, Structure* structure, Butterfly* butterfly)
    : Base(vm, structure, butterfly)
{
}

ClonedArguments* ClonedArguments::createEmpty(VM& vm, JSGlobalObject* nullOrGlobalObjectForOOM, Structure* structure, JSFunction* callee, unsigned length, Butterfly* butterfly)
{
    unsigned vectorLength = length;
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH)
        return nullptr;

    if (UNLIKELY(structure->mayInterceptIndexedAccesses() || structure->storedPrototypeObject()->needsSlowPutIndexing())) {
        if (!butterfly) {
            butterfly = tryCreateArrayStorageButterfly(vm, nullptr, structure, length, vectorLength);
            if (UNLIKELY(!butterfly)) {
                if (nullOrGlobalObjectForOOM) {
                    auto scope = DECLARE_THROW_SCOPE(vm);
                    throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
                }
                return nullptr;
            }
            butterfly->arrayStorage()->m_numValuesInVector = vectorLength;
        }
    } else {
        if (!butterfly) {
            IndexingHeader indexingHeader;
            indexingHeader.setVectorLength(vectorLength);
            indexingHeader.setPublicLength(length);
            butterfly = Butterfly::tryCreate(vm, nullptr, 0, structure->outOfLineCapacity(), true, indexingHeader, vectorLength * sizeof(EncodedJSValue));
            if (UNLIKELY(!butterfly)) {
                if (nullOrGlobalObjectForOOM) {
                    auto scope = DECLARE_THROW_SCOPE(vm);
                    throwOutOfMemoryError(nullOrGlobalObjectForOOM, scope);
                }
                return nullptr;
            }
        }

        for (unsigned i = length; i < vectorLength; ++i)
            butterfly->contiguous().atUnsafe(i).clear();
    }
    
    ClonedArguments* result =
        new (NotNull, allocateCell<ClonedArguments>(vm))
        ClonedArguments(vm, structure, butterfly);
    result->finishCreation(vm);

    result->m_callee.set(vm, result, callee);
    result->putDirectOffset(vm, clonedArgumentsLengthPropertyOffset, jsNumber(length));
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

    auto createEmptyWithAssert = [&](JSGlobalObject* globalObject, JSFunction* callee, unsigned length) {
        // NB. Some clients might expect that the global object of of this object is the global object
        // of the callee. We don't do this for now, but maybe we should.
        ClonedArguments* result = ClonedArguments::createEmpty(globalObject->vm(), globalObject, globalObject->clonedArgumentsStructure(), callee, length, nullptr);
        ASSERT(!result || !result->needsSlowPutIndexing() || shouldUseSlowPut(result->structure()->indexingType()));
        return result;
    };
    
    unsigned length = 0; // Initialize because VC needs it.
    switch (mode) {
    case ArgumentsMode::Cloned: {
        if (inlineCallFrame) {
            if (inlineCallFrame->argumentCountRegister.isValid())
                length = targetFrame->r(inlineCallFrame->argumentCountRegister).unboxedInt32();
            else
                length = inlineCallFrame->argumentCountIncludingThis;
            length--;
            result = createEmptyWithAssert(globalObject, callee, length);
            if (!result)
                return result;

            for (unsigned i = length; i--;)
                result->putDirectIndex(globalObject, i, inlineCallFrame->m_argumentsWithFixup[i + 1].recover(targetFrame));
        } else {
            length = targetFrame->argumentCount();
            result = createEmptyWithAssert(globalObject, callee, length);
            if (!result)
                return result;

            for (unsigned i = length; i--;)
                result->putDirectIndex(globalObject, i, targetFrame->uncheckedArgument(i));
        }
        break;
    }
        
    case ArgumentsMode::FakeValues: {
        result = createEmptyWithAssert(globalObject, callee, 0);
        if (!result)
            return result;
        break;
    } }

    ASSERT(globalObject->clonedArgumentsStructure() == result->structure());
    ASSERT(!result->needsSlowPutIndexing() || shouldUseSlowPut(result->structure()->indexingType()));
    return result;
}

ClonedArguments* ClonedArguments::createWithMachineFrame(JSGlobalObject* globalObject, CallFrame* targetFrame, ArgumentsMode mode)
{
    ClonedArguments* result = createWithInlineFrame(globalObject, targetFrame, nullptr, mode);
    ASSERT(!result || !result->needsSlowPutIndexing() || shouldUseSlowPut(result->structure()->indexingType()));
    return result;
}

ClonedArguments* ClonedArguments::createByCopyingFrom(JSGlobalObject* globalObject, Structure* structure, Register* argumentStart, unsigned length, JSFunction* callee, Butterfly* butterfly)
{
    ClonedArguments* result = createEmpty(globalObject->vm(), globalObject, structure, callee, length, butterfly);
    if (!result)
        return result;
    
    for (unsigned i = length; i--;)
        result->putDirectIndex(globalObject, i, argumentStart[i].jsValue());
    ASSERT(!result->needsSlowPutIndexing() || shouldUseSlowPut(result->structure()->indexingType()));
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
        bool isStrictMode = executable->isInStrictContext();

        if (ident == vm.propertyNames->callee) {
            if (isStrictMode || executable->usesNonSimpleParameterList()) {
                slot.setGetterSlot(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::Accessor, thisObject->globalObject()->throwTypeErrorArgumentsCalleeGetterSetter());
                return true;
            }
            // This happens when ClonedArguments is created for non strict functions via `func.arguments` access.
            slot.setValue(thisObject, 0, thisObject->m_callee.get());
            return true;
        }

        if (ident == vm.propertyNames->iteratorSymbol) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum), thisObject->globalObject()->arrayProtoValuesFunction());
            return true;
        }
    }

    return Base::getOwnPropertySlot(thisObject, globalObject, ident, slot);
}

void ClonedArguments::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray&, DontEnumPropertiesMode mode)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(object);
    if (mode == DontEnumPropertiesMode::Include)
        thisObject->materializeSpecialsIfNecessary(globalObject);
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

bool ClonedArguments::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName ident, DeletePropertySlot& slot)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(cell);
    VM& vm = globalObject->vm();
    
    if (ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol)
        thisObject->materializeSpecialsIfNecessary(globalObject);
    
    return Base::deleteProperty(thisObject, globalObject, ident, slot);
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
    bool isStrictMode = executable->isInStrictContext();
    
    if (isStrictMode || executable->usesNonSimpleParameterList())
        putDirectAccessor(globalObject, vm.propertyNames->callee, this->globalObject()->throwTypeErrorArgumentsCalleeGetterSetter(), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    else
        putDirect(vm, vm.propertyNames->callee, JSValue(m_callee.get()));

    putDirect(vm, vm.propertyNames->iteratorSymbol, this->globalObject()->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    m_callee.clear();
}

void ClonedArguments::materializeSpecialsIfNecessary(JSGlobalObject* globalObject)
{
    if (!specialsMaterialized())
        materializeSpecials(globalObject);
}

template<typename Visitor>
void ClonedArguments::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ClonedArguments* thisObject = jsCast<ClonedArguments*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_callee);
}

DEFINE_VISIT_CHILDREN(ClonedArguments);

void ClonedArguments::copyToArguments(JSGlobalObject* globalObject, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (this->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        auto& butterfly = *this->butterfly();
        auto data = butterfly.contiguous().data();
        unsigned limit = std::min(length + offset, butterfly.vectorLength());
        unsigned i;
        for (i = offset; i < limit; ++i) {
            JSValue value = data[i].get();
            if (UNLIKELY(!value)) {
                value = get(globalObject, i);
                RETURN_IF_EXCEPTION(scope, void());
            }
            firstElementDest[i - offset] = value;
        }
        for (; i < length; ++i) {
            firstElementDest[i - offset] = get(globalObject, i);
            RETURN_IF_EXCEPTION(scope, void());
        }
        return;
    }
    default:
        break;
    }

    unsigned i;
    for (i = 0; i < length && canGetIndexQuickly(i + offset); ++i)
        firstElementDest[i] = getIndexQuickly(i + offset);
    for (; i < length; ++i) {
        JSValue value = get(globalObject, i + offset);
        RETURN_IF_EXCEPTION(scope, void());
        firstElementDest[i] = value;
    }
}

bool ClonedArguments::isIteratorProtocolFastAndNonObservable()
{
    Structure* structure = this->structure();
    JSGlobalObject* globalObject = structure->globalObject();
    if (!globalObject->isArgumentsPrototypeIteratorProtocolFastAndNonObservable())
        return false;

    // FIXME: We should relax this restriction, or reorganize ClonedArguments's @@iterator property materialization to go to this condition.
    // Probably, we should make ClonedArguments more similar to DirectArguments, and tracking changes on them instead of using relatively plain objects.
    if (structure->didTransition())
        return false;

    if (UNLIKELY(structure->mayInterceptIndexedAccesses() || structure->storedPrototypeObject()->needsSlowPutIndexing()))
        return false;

    // Even though Structure is not transitioned, it is possible that length property is replaced with random value.
    // To avoid side-effect, we need to ensure that this value is Int32.
    JSValue lengthValue = getDirect(clonedArgumentsLengthPropertyOffset);
    if (LIKELY(lengthValue.isInt32() && lengthValue.asInt32() >= 0))
        return true;

    return false;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
