/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "FTLOperations.h"

#if ENABLE(FTL_JIT)

#include "BytecodeStructs.h"
#include "ClonedArguments.h"
#include "CommonSlowPaths.h"
#include "DirectArguments.h"
#include "FTLJITCode.h"
#include "FTLLazySlowPath.h"
#include "FrameTracers.h"
#include "InlineCallFrame.h"
#include "Interpreter.h"
#include "JSArrayIterator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "RegExpObject.h"

IGNORE_WARNINGS_BEGIN("frame-address")

namespace JSC { namespace FTL {

extern "C" void JIT_OPERATION operationPopulateObjectInOSR(JSGlobalObject* globalObject, ExitTimeObjectMaterialization* materialization, EncodedJSValue* encodedValue, EncodedJSValue* values)
{
    using namespace DFG;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    CodeBlock* codeBlock = callFrame->codeBlock();

    // We cannot GC. We've got pointers in evil places.
    // FIXME: We are not doing anything that can GC here, and this is
    // probably unnecessary.
    DeferGCForAWhile deferGC(vm.heap);

    switch (materialization->type()) {
    case PhantomNewObject: {
        JSFinalObject* object = jsCast<JSFinalObject*>(JSValue::decode(*encodedValue));
        Structure* structure = object->structure(vm);

        // Figure out what the heck to populate the object with. Use
        // getPropertiesConcurrently() because that happens to be
        // lower-level and more convenient. It doesn't change the
        // materialization of the property table. We want to have
        // minimal visible effects on the system. Also, don't mind
        // that this is O(n^2). It doesn't matter. We only get here
        // from OSR exit.
        for (PropertyMapEntry entry : structure->getPropertiesConcurrently()) {
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location().kind() != NamedPropertyPLoc)
                    continue;
                if (codeBlock->identifier(property.location().info()).impl() != entry.key)
                    continue;

                object->putDirect(vm, entry.offset, JSValue::decode(values[i]));
            }
        }
        break;
    }

    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
        // Those are completely handled by operationMaterializeObjectInOSR
        break;

    case PhantomCreateActivation: {
        JSLexicalEnvironment* activation = jsCast<JSLexicalEnvironment*>(JSValue::decode(*encodedValue));

        // Figure out what to populate the activation with
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() != ClosureVarPLoc)
                continue;

            activation->variableAt(ScopeOffset(property.location().info())).set(vm, activation, JSValue::decode(values[i]));
        }

        break;
    }

    case PhantomNewArrayIterator: {
        JSArrayIterator* arrayIterator = jsCast<JSArrayIterator*>(JSValue::decode(*encodedValue));

        // Figure out what to populate the iterator with
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() != InternalFieldObjectPLoc)
                continue;
            arrayIterator->internalField(static_cast<JSArrayIterator::Field>(property.location().info())).set(vm, arrayIterator, JSValue::decode(values[i]));
        }
        break;
    }

    case PhantomNewRegexp: {
        RegExpObject* regExpObject = jsCast<RegExpObject*>(JSValue::decode(*encodedValue));

        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() != RegExpObjectLastIndexPLoc)
                continue;

            regExpObject->setLastIndex(globalObject, JSValue::decode(values[i]), false /* shouldThrow */);
            break;
        }
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;

    }
}

extern "C" JSCell* JIT_OPERATION operationMaterializeObjectInOSR(JSGlobalObject* globalObject, ExitTimeObjectMaterialization* materialization, EncodedJSValue* values)
{
    using namespace DFG;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);

    // We cannot GC. We've got pointers in evil places.
    DeferGCForAWhile deferGC(vm.heap);
    
    switch (materialization->type()) {
    case PhantomNewObject: {
        // Figure out what the structure is
        Structure* structure = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location() != PromotedLocationDescriptor(StructurePLoc))
                continue;

            RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<Structure>(vm));
            structure = jsCast<Structure*>(JSValue::decode(values[i]));
            break;
        }
        RELEASE_ASSERT(structure);

        JSFinalObject* result = JSFinalObject::create(vm, structure);

        // The real values will be put subsequently by
        // operationPopulateNewObjectInOSR. We can't fill them in
        // now, because they may not be available yet (typically
        // because we have a cyclic dependency graph).

        // We put a dummy value here in order to avoid super-subtle
        // GC-and-OSR-exit crashes in case we have a bug and some
        // field is, for any reason, not filled later.
        // We use a random-ish number instead of a sensible value like
        // undefined to make possible bugs easier to track.
        for (PropertyMapEntry entry : structure->getPropertiesConcurrently())
            result->putDirect(vm, entry.offset, jsNumber(19723));

        return result;
    }

    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewAsyncFunction: {
        // Figure out what the executable and activation are
        FunctionExecutable* executable = nullptr;
        JSScope* activation = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location() == PromotedLocationDescriptor(FunctionExecutablePLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<FunctionExecutable>(vm));
                executable = jsCast<FunctionExecutable*>(JSValue::decode(values[i]));
            }
            if (property.location() == PromotedLocationDescriptor(FunctionActivationPLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<JSScope>(vm));
                activation = jsCast<JSScope*>(JSValue::decode(values[i]));
            }
        }
        RELEASE_ASSERT(executable && activation);

        if (materialization->type() == PhantomNewFunction)
            return JSFunction::createWithInvalidatedReallocationWatchpoint(vm, executable, activation);
        else if (materialization->type() == PhantomNewGeneratorFunction)
            return JSGeneratorFunction::createWithInvalidatedReallocationWatchpoint(vm, executable, activation);    
        else if (materialization->type() == PhantomNewAsyncGeneratorFunction)
            return JSAsyncGeneratorFunction::createWithInvalidatedReallocationWatchpoint(vm, executable, activation);
        ASSERT(materialization->type() == PhantomNewAsyncFunction);
        return JSAsyncFunction::createWithInvalidatedReallocationWatchpoint(vm, executable, activation);
    }

    case PhantomCreateActivation: {
        // Figure out what the scope and symbol table are
        JSScope* scope = nullptr;
        SymbolTable* table = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location() == PromotedLocationDescriptor(ActivationScopePLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<JSScope>(vm));
                scope = jsCast<JSScope*>(JSValue::decode(values[i]));
            } else if (property.location() == PromotedLocationDescriptor(ActivationSymbolTablePLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<SymbolTable>(vm));
                table = jsCast<SymbolTable*>(JSValue::decode(values[i]));
            }
        }
        RELEASE_ASSERT(scope);
        RELEASE_ASSERT(table);

        CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(
            materialization->origin(), callFrame->codeBlock()->baselineAlternative());
        Structure* structure = codeBlock->globalObject()->activationStructure();

        // It doesn't matter what values we initialize as bottom values inside the activation constructor because
        // activation sinking will set bottom values for each slot.
        // FIXME: Slight optimization would be to create a constructor that doesn't initialize all slots.
        JSLexicalEnvironment* result = JSLexicalEnvironment::create(vm, structure, scope, table, jsUndefined());

        RELEASE_ASSERT(materialization->properties().size() - 2 == table->scopeSize());

        // The real values will be put subsequently by
        // operationPopulateNewObjectInOSR. See the PhantomNewObject
        // case for details.
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() != ClosureVarPLoc)
                continue;

            result->variableAt(ScopeOffset(property.location().info())).set(
                vm, result, jsNumber(29834));
        }

        if (validationEnabled()) {
            // Validate to make sure every slot in the scope has one value.
            ConcurrentJSLocker locker(table->m_lock);
            for (auto iter = table->begin(locker), end = table->end(locker); iter != end; ++iter) {
                bool found = false;
                for (unsigned i = materialization->properties().size(); i--;) {
                    const ExitPropertyValue& property = materialization->properties()[i];
                    if (property.location().kind() != ClosureVarPLoc)
                        continue;
                    if (ScopeOffset(property.location().info()) == iter->value.scopeOffset()) {
                        found = true;
                        break;
                    }
                }
                ASSERT_UNUSED(found, found);
            }
            unsigned numberOfClosureVarPloc = 0;
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location().kind() == ClosureVarPLoc)
                    numberOfClosureVarPloc++;
            }
            ASSERT(numberOfClosureVarPloc == table->scopeSize());
        }

        return result;
    }

    case PhantomNewArrayIterator: {
        // Figure out what structure.
        Structure* structure = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location() == PromotedLocationDescriptor(StructurePLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<Structure>(vm));
                structure = jsCast<Structure*>(JSValue::decode(values[i]));
            }
        }
        RELEASE_ASSERT(structure);

        JSArrayIterator* result = JSArrayIterator::createWithInitialValues(vm, structure);

        RELEASE_ASSERT(materialization->properties().size() - 1 == JSArrayIterator::numberOfInternalFields);

        // The real values will be put subsequently by
        // operationPopulateNewObjectInOSR. See the PhantomNewObject
        // case for details.
        return result;
    }

    case PhantomCreateRest:
    case PhantomDirectArguments:
    case PhantomClonedArguments: {
        if (!materialization->origin().inlineCallFrame()) {
            switch (materialization->type()) {
            case PhantomDirectArguments:
                return DirectArguments::createByCopying(globalObject, callFrame);
            case PhantomClonedArguments:
                return ClonedArguments::createWithMachineFrame(globalObject, callFrame, ArgumentsMode::Cloned);
            case PhantomCreateRest: {
                CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(
                    materialization->origin(), callFrame->codeBlock()->baselineAlternative());

                unsigned numberOfArgumentsToSkip = codeBlock->numberOfArgumentsToSkip();
                JSGlobalObject* globalObject = codeBlock->globalObject();
                Structure* structure = globalObject->restParameterStructure();
                JSValue* argumentsToCopyRegion = callFrame->addressOfArgumentsStart() + numberOfArgumentsToSkip;
                unsigned arraySize = callFrame->argumentCount() > numberOfArgumentsToSkip ? callFrame->argumentCount() - numberOfArgumentsToSkip : 0;
                return constructArray(globalObject, structure, argumentsToCopyRegion, arraySize);
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return nullptr;
            }
        }

        // First figure out the argument count. If there isn't one then we represent the machine frame.
        unsigned argumentCount = 0;
        if (materialization->origin().inlineCallFrame()->isVarargs()) {
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location() != PromotedLocationDescriptor(ArgumentCountPLoc))
                    continue;
                argumentCount = JSValue::decode(values[i]).asUInt32();
                break;
            }
        } else
            argumentCount = materialization->origin().inlineCallFrame()->argumentCountIncludingThis;
        RELEASE_ASSERT(argumentCount);
        
        JSFunction* callee = nullptr;
        if (materialization->origin().inlineCallFrame()->isClosureCall) {
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location() != PromotedLocationDescriptor(ArgumentsCalleePLoc))
                    continue;
                
                callee = jsCast<JSFunction*>(JSValue::decode(values[i]));
                break;
            }
        } else
            callee = materialization->origin().inlineCallFrame()->calleeConstant();
        RELEASE_ASSERT(callee);
        
        CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(
            materialization->origin(), callFrame->codeBlock()->baselineAlternative());
        
        // We have an inline frame and we have all of the data we need to recreate it.
        switch (materialization->type()) {
        case PhantomDirectArguments: {
            unsigned length = argumentCount - 1;
            unsigned capacity = std::max(length, static_cast<unsigned>(codeBlock->numParameters() - 1));
            DirectArguments* result = DirectArguments::create(
                vm, codeBlock->globalObject()->directArgumentsStructure(), length, capacity);
            result->setCallee(vm, callee);
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location().kind() != ArgumentPLoc)
                    continue;
                
                unsigned index = property.location().info();
                if (index >= capacity)
                    continue;
                
                // We don't want to use setIndexQuickly(), since that's only for the passed-in
                // arguments but sometimes the number of named arguments is greater. For
                // example:
                //
                // function foo(a, b, c) { ... }
                // foo();
                //
                // setIndexQuickly() would fail for indices 0, 1, 2 - but we need to recover
                // those here.
                result->argument(DirectArgumentsOffset(index)).set(
                    vm, result, JSValue::decode(values[i]));
            }
            return result;
        }
        case PhantomClonedArguments: {
            unsigned length = argumentCount - 1;
            ClonedArguments* result = ClonedArguments::createEmpty(
                vm, codeBlock->globalObject()->clonedArgumentsStructure(), callee, length);
            
            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location().kind() != ArgumentPLoc)
                    continue;
                
                unsigned index = property.location().info();
                if (index >= length)
                    continue;
                result->putDirectIndex(globalObject, index, JSValue::decode(values[i]));
            }
            
            return result;
        }
        case PhantomCreateRest: {
            unsigned numberOfArgumentsToSkip = codeBlock->numberOfArgumentsToSkip();
            JSGlobalObject* globalObject = codeBlock->globalObject();
            Structure* structure = globalObject->restParameterStructure();
            ASSERT(argumentCount > 0);
            unsigned arraySize = (argumentCount - 1) > numberOfArgumentsToSkip ? argumentCount - 1 - numberOfArgumentsToSkip : 0;

            // FIXME: we should throw an out of memory error here if tryCreate() fails.
            // https://bugs.webkit.org/show_bug.cgi?id=169784
            JSArray* array = JSArray::tryCreate(vm, structure, arraySize);
            RELEASE_ASSERT(array);

            for (unsigned i = materialization->properties().size(); i--;) {
                const ExitPropertyValue& property = materialization->properties()[i];
                if (property.location().kind() != ArgumentPLoc)
                    continue;

                unsigned argIndex = property.location().info();
                if (numberOfArgumentsToSkip > argIndex)
                    continue;
                unsigned arrayIndex = argIndex - numberOfArgumentsToSkip;
                if (arrayIndex >= arraySize)
                    continue;
                array->putDirectIndex(globalObject, arrayIndex, JSValue::decode(values[i]));
            }

#if ASSERT_ENABLED
            // We avoid this O(n^2) loop when asserts are disabled, but the condition checked here
            // must hold to ensure the correctness of the above loop because of how we allocate the array.
            for (unsigned targetIndex = 0; targetIndex < arraySize; ++targetIndex) {
                bool found = false;
                for (unsigned i = materialization->properties().size(); i--;) {
                    const ExitPropertyValue& property = materialization->properties()[i];
                    if (property.location().kind() != ArgumentPLoc)
                        continue;

                    unsigned argIndex = property.location().info();
                    if (numberOfArgumentsToSkip > argIndex)
                        continue;
                    unsigned arrayIndex = argIndex - numberOfArgumentsToSkip;
                    if (arrayIndex >= arraySize)
                        continue;
                    if (arrayIndex == targetIndex) {
                        found = true;
                        break;
                    }
                }
                ASSERT(found);
            }
#endif // ASSERT_ENABLED
            return array;
        }

        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    case PhantomSpread: {
        JSArray* array = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() == SpreadPLoc) {
                array = jsCast<JSArray*>(JSValue::decode(values[i]));
                break;
            }
        }
        RELEASE_ASSERT(array);

        // Note: it is sound for JSImmutableButterfly::createFromArray to call getDirectIndex here
        // because we're guaranteed we won't be calling any getters. The reason for this is
        // that we only support PhantomSpread over CreateRest, which is an array we create.
        // Any attempts to put a getter on any indices on the rest array will escape the array.
        auto* fixedArray = JSImmutableButterfly::createFromArray(globalObject, vm, array);
        RELEASE_ASSERT(fixedArray);
        return fixedArray;
    }

    case PhantomNewArrayBuffer: {
        JSImmutableButterfly* immutableButterfly = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() == NewArrayBufferPLoc) {
                immutableButterfly = jsCast<JSImmutableButterfly*>(JSValue::decode(values[i]));
                break;
            }
        }
        RELEASE_ASSERT(immutableButterfly);

        // For now, we use array allocation profile in the actual CodeBlock. It is OK since current NewArrayBuffer
        // and PhantomNewArrayBuffer are always bound to a specific op_new_array_buffer.
        CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(materialization->origin(), callFrame->codeBlock()->baselineAlternative());
        const Instruction* currentInstruction = codeBlock->instructions().at(materialization->origin().bytecodeIndex()).ptr();
        if (!currentInstruction->is<OpNewArrayBuffer>()) {
            // This case can happen if Object.keys, an OpCall is first converted into a NewArrayBuffer which is then converted into a PhantomNewArrayBuffer.
            // There is no need to update the array allocation profile in that case.
            RELEASE_ASSERT(currentInstruction->is<OpCall>());
            Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(immutableButterfly->indexingMode());
            return CommonSlowPaths::allocateNewArrayBuffer(vm, structure, immutableButterfly);
        }
        auto newArrayBuffer = currentInstruction->as<OpNewArrayBuffer>();
        ArrayAllocationProfile* profile = &newArrayBuffer.metadata(codeBlock).m_arrayAllocationProfile;

        // FIXME: Share the code with CommonSlowPaths. Currently, codeBlock etc. are slightly different.
        IndexingType indexingMode = profile->selectIndexingType();
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingMode);
        ASSERT(isCopyOnWrite(indexingMode));
        ASSERT(!structure->outOfLineCapacity());

        if (UNLIKELY(immutableButterfly->indexingMode() != indexingMode)) {
            auto* newButterfly = JSImmutableButterfly::create(vm, indexingMode, immutableButterfly->length());
            for (unsigned i = 0; i < immutableButterfly->length(); ++i)
                newButterfly->setIndex(vm, i, immutableButterfly->get(i));
            immutableButterfly = newButterfly;

            // FIXME: This is kinda gross and only works because we can't inline new_array_bufffer in the baseline.
            // We also cannot allocate a new butterfly from compilation threads since it's invalid to allocate cells from
            // a compilation thread.
            WTF::storeStoreFence();
            codeBlock->constantRegister(newArrayBuffer.m_immutableButterfly).set(vm, codeBlock, immutableButterfly);
            WTF::storeStoreFence();
        }

        JSArray* result = CommonSlowPaths::allocateNewArrayBuffer(vm, structure, immutableButterfly);
        ArrayAllocationProfile::updateLastAllocationFor(profile, result);
        return result;
    }

    case PhantomNewArrayWithSpread: {
        CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(
            materialization->origin(), callFrame->codeBlock()->baselineAlternative());
        JSGlobalObject* globalObject = codeBlock->globalObject();
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

        Checked<unsigned, RecordOverflow> checkedArraySize = 0;
        unsigned numProperties = 0;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() == NewArrayWithSpreadArgumentPLoc) {
                ++numProperties;
                JSValue value = JSValue::decode(values[i]);
                if (JSImmutableButterfly* immutableButterfly = jsDynamicCast<JSImmutableButterfly*>(vm, value))
                    checkedArraySize += immutableButterfly->publicLength();
                else
                    checkedArraySize += 1;
            }
        }

        // FIXME: we should throw an out of memory error here if checkedArraySize has hasOverflowed() or tryCreate() fails.
        // https://bugs.webkit.org/show_bug.cgi?id=169784
        unsigned arraySize = checkedArraySize.unsafeGet(); // Crashes if overflowed.
        JSArray* result = JSArray::tryCreate(vm, structure, arraySize);
        RELEASE_ASSERT(result);

#if ASSERT_ENABLED
        // Ensure we see indices for everything in the range: [0, numProperties)
        for (unsigned i = 0; i < numProperties; ++i) {
            bool found = false;
            for (unsigned j = 0; j < materialization->properties().size(); ++j) {
                const ExitPropertyValue& property = materialization->properties()[j];
                if (property.location().kind() == NewArrayWithSpreadArgumentPLoc && property.location().info() == i) {
                    found = true;
                    break;
                }
            }
            ASSERT(found);
        }
#endif // ASSERT_ENABLED

        Vector<JSValue, 8> arguments;
        arguments.grow(numProperties);

        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location().kind() == NewArrayWithSpreadArgumentPLoc) {
                JSValue value = JSValue::decode(values[i]);
                RELEASE_ASSERT(property.location().info() < numProperties);
                arguments[property.location().info()] = value;
            }
        }

        unsigned arrayIndex = 0;
        for (JSValue value : arguments) {
            if (JSImmutableButterfly* immutableButterfly = jsDynamicCast<JSImmutableButterfly*>(vm, value)) {
                for (unsigned i = 0; i < immutableButterfly->publicLength(); i++) {
                    ASSERT(immutableButterfly->get(i));
                    result->putDirectIndex(globalObject, arrayIndex, immutableButterfly->get(i));
                    ++arrayIndex;
                }
            } else {
                // We are not spreading.
                result->putDirectIndex(globalObject, arrayIndex, value);
                ++arrayIndex;
            }
        }

        return result;
    }

    case PhantomNewRegexp: {
        RegExp* regExp = nullptr;
        for (unsigned i = materialization->properties().size(); i--;) {
            const ExitPropertyValue& property = materialization->properties()[i];
            if (property.location() == PromotedLocationDescriptor(RegExpObjectRegExpPLoc)) {
                RELEASE_ASSERT(JSValue::decode(values[i]).asCell()->inherits<RegExp>(vm));
                regExp = jsCast<RegExp*>(JSValue::decode(values[i]));
            }
        }
        RELEASE_ASSERT(regExp);
        CodeBlock* codeBlock = baselineCodeBlockForOriginAndBaselineCodeBlock(materialization->origin(), callFrame->codeBlock()->baselineAlternative());
        Structure* structure = codeBlock->globalObject()->regExpStructure();
        return RegExpObject::create(vm, structure, regExp);
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

extern "C" void* JIT_OPERATION operationCompileFTLLazySlowPath(CallFrame* callFrame, unsigned index)
{
    VM& vm = callFrame->deprecatedVM();

    // We cannot GC. We've got pointers in evil places.
    DeferGCForAWhile deferGC(vm.heap);

    CodeBlock* codeBlock = callFrame->codeBlock();
    JITCode* jitCode = codeBlock->jitCode()->ftl();

    LazySlowPath& lazySlowPath = *jitCode->lazySlowPaths[index];
    lazySlowPath.generate(codeBlock);

    return lazySlowPath.stub().code().executableAddress();
}

} } // namespace JSC::FTL

IGNORE_WARNINGS_END

#endif // ENABLE(FTL_JIT)

