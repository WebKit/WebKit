/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#if ENABLE(MOVABLE_GC_OBJECTS)
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
#include "JIT.h"

#include "CodeBlock.h"
#include "Interpreter.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSObject.h"
#include "JSPropertyNameIterator.h"
#include "LinkBuffer.h"
#include "PropertySlot.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"

using namespace std;

namespace JSC {

typedef PropertySlot::CachedPropertyType PropertyType;

void JIT::patchPrototypeStructureAddress(CodeLocationDataLabelPtr where, MarkStack& markStack, RepatchBuffer& repatchBuffer)
{
    uintptr_t prototypeStructureAddress = reinterpret_cast<uintptr_t>(pointerAtLocation(where));
    JSCell* cell = reinterpret_cast<JSCell*>(prototypeStructureAddress & CELL_ALIGN_MASK);

    ASSERT(&(cell->m_structure) == pointerAtLocation(where));
    markStack.append(cell);
    repatchBuffer.repatch(where, &(cell->m_structure));
}

void JIT::patchGetDirectOffset(CodeLocationLabel patchStart, MarkStack& markStack, RepatchBuffer& repatchBuffer, PropertyType propertyType)
{
    CodeLocationDataLabelPtr where;
    switch (propertyType) {
    case PropertySlot::Getter:
        where = patchStart.dataLabelPtrAtOffset(patchLengthStore);
        break;
    case PropertySlot::Custom:
        where = patchStart.dataLabelPtrAtOffset(patchLengthMove);
        break;
    default:
        where = patchStart.dataLabelPtrAtOffset(patchLengthMove);
        break;
    }

    uintptr_t propertyAddress;
    uintptr_t newPropertyAddress;
    ptrdiff_t offset;
    JSObject* object;

#if USE(JSVALUE32_64)
    // JSVALUE32_64 will need to repatch two pointers for 32 bit code
    propertyAddress = reinterpret_cast<uintptr_t>(pointerAtLocation(where));
    object = reinterpret_cast<JSObject*>(propertyAddress & CELL_ALIGN_MASK);
    offset = propertyAddress & CELL_MASK;
    markStack.append(object);
    newPropertyAddress = reinterpret_cast<uintptr_t>(object) + offset;
    repatchBuffer.repatch(where, reinterpret_cast<void*>(newPropertyAddress));

    if (offset == OBJECT_OFFSETOF(JSObject, m_externalStorage))
        return;

    ASSERT(object->isUsingInlineStorage());
    ASSERT(offset >= OBJECT_OFFSETOF(JSObject, m_inlineStorage));
    ASSERT(offset < OBJECT_OFFSETOF(JSObject, m_inlineStorage[JSObject::inlineStorageCapacity]));

    where = where.dataLabelPtrAtOffset(patchLengthMove);
#endif

    propertyAddress = reinterpret_cast<uintptr_t>(pointerAtLocation(where));
    object = reinterpret_cast<JSObject*>(propertyAddress & CELL_ALIGN_MASK);
    offset = propertyAddress & CELL_MASK;
    markStack.append(object);
    newPropertyAddress = reinterpret_cast<uintptr_t>(object) + offset;
    repatchBuffer.repatch(where, reinterpret_cast<void*>(newPropertyAddress));
}

void JIT::markGetByIdProto(MarkStack& markStack, CodeBlock* codeBlock, StructureStubInfo* stubInfo)
{
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    CodeLocationLabel patchStart(jumpTarget(jumpLocation));

    RepatchBuffer repatchBuffer(codeBlock);

    CodeLocationDataLabelPtr where = patchStart.dataLabelPtrAtOffset(patchOffsetGetByIdProtoStruct);
    patchPrototypeStructureAddress(patchStart.dataLabelPtrAtOffset(patchOffsetGetByIdProtoStruct), markStack, repatchBuffer);

    patchGetDirectOffset(where.labelAtOffset(patchLengthBranchPtr), markStack, repatchBuffer, stubInfo->u.getByIdProto.propertyType);
}

void JIT::markGetByIdChain(MarkStack& markStack, CodeBlock* codeBlock, StructureStubInfo* stubInfo)
{
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    CodeLocationLabel patchStart(jumpTarget(jumpLocation));

    RepatchBuffer repatchBuffer(codeBlock);

    int count = stubInfo->u.getByIdChain.count;
    for (int i = 0; i < count; ++i) {
        CodeLocationDataLabelPtr where = patchStart.dataLabelPtrAtOffset(patchOffsetGetByIdProtoStruct + patchLengthTestPrototype * i);
        patchPrototypeStructureAddress(where, markStack, repatchBuffer);
    }
    CodeLocationLabel where =
            patchStart.labelAtOffset(patchOffsetGetByIdProtoStruct + patchLengthBranchPtr + patchLengthTestPrototype * (count - 1));
    patchGetDirectOffset(where, markStack, repatchBuffer, stubInfo->u.getByIdChain.propertyType);
}

void JIT::markGetByIdProtoList(MarkStack& markStack, CodeBlock* codeBlock, StructureStubInfo* stubInfo)
{
    RepatchBuffer repatchBuffer(codeBlock);

    for (int i = 0; i < stubInfo->u.getByIdSelfList.listSize; ++i) {
        CodeLocationLabel patchStart = stubInfo->u.getByIdProtoList.structureList->list[i].stubRoutine;

        PolymorphicAccessStructureList::PolymorphicStubInfo info =
            stubInfo->u.getByIdProtoList.structureList->list[i];

        if (!info.u.proto)
            continue;

        if (info.isChain) {
            int count = info.count;
            for (int j = 0; j < count; ++j) {
                CodeLocationDataLabelPtr where = patchStart.dataLabelPtrAtOffset(patchOffsetGetByIdProtoStruct + patchLengthTestPrototype * j);
                patchPrototypeStructureAddress(where, markStack, repatchBuffer);
            }
            CodeLocationLabel where =
                    patchStart.labelAtOffset(patchOffsetGetByIdProtoStruct + patchLengthBranchPtr + patchLengthTestPrototype * (count - 1));
            patchGetDirectOffset(where, markStack, repatchBuffer, info.propertyType);
        } else {
            CodeLocationDataLabelPtr where = patchStart.dataLabelPtrAtOffset(patchOffsetGetByIdProtoStruct);
            patchPrototypeStructureAddress(where, markStack, repatchBuffer);

            patchGetDirectOffset(where.labelAtOffset(patchLengthBranchPtr), markStack, repatchBuffer, info.propertyType);
        }
    }
}

void JIT::markPutByIdTransition(MarkStack& markStack, CodeBlock* codeBlock, StructureStubInfo* stubInfo)
{
    CodeLocationLabel patchStart = stubInfo->stubRoutine;

    RepatchBuffer repatchBuffer(codeBlock);

    RefPtr<Structure>* it = stubInfo->u.putByIdTransition.chain->head();
    JSValue protoObject = stubInfo->u.putByIdTransition.structure->storedPrototype();

    do {
        if (protoObject.isNull())
            continue;

        CodeLocationDataLabelPtr where = patchStart.dataLabelPtrAtOffset(patchOffsetPutByIdProtoStruct + patchLengthTestPrototype);
        patchPrototypeStructureAddress(where, markStack, repatchBuffer);

        protoObject = it->get()->storedPrototype();
        patchStart = patchStart.labelAtOffset(patchLengthTestPrototype);
    } while (*it++);
}

void JIT::markGlobalObjectReference(MarkStack& markStack, CodeBlock* codeBlock, CodeLocationDataLabelPtr ref)
{
    RepatchBuffer repatchBuffer(codeBlock);
    JSCell* globalObject = reinterpret_cast<JSCell*>(pointerAtLocation(ref));

    ASSERT(!(reinterpret_cast<uintptr_t>(globalObject) & CELL_MASK));

    markStack.append(globalObject);
    repatchBuffer.repatch(ref, globalObject);
}

} // namespace JSC

#endif

#endif // ENABLE(MOVABLE_GC_OBJECTS) 
#endif // ENABLE(JIT)
