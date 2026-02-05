/*
 * Copyright (C) 2026 Anthropic PBC.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IntlSegmentDataObject.h"

#include "JSCInlines.h"

namespace JSC {

Structure* createSegmentDataObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 3;
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->segment, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectSegmentPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->index, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectIndexPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->input, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectInputPropertyOffset);
    return structure;
}

Structure* createSegmentDataObjectWithIsWordLikeStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 4;
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->segment, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectSegmentPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->index, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectIndexPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->input, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectInputPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->isWordLike, 0, offset);
    RELEASE_ASSERT(offset == segmentDataObjectIsWordLikePropertyOffset);
    return structure;
}

} // namespace JSC
