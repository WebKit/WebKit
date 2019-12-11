/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSMap.h"

#include "JSCInlines.h"
#include "MapPrototype.h"

namespace JSC {

const ClassInfo JSMap::s_info = { "Map", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSMap) };

String JSMap::toStringName(const JSObject*, JSGlobalObject*)
{
    return "Object"_s;
}

JSMap* JSMap::clone(JSGlobalObject* globalObject, VM& vm, Structure* structure)
{
    JSMap* instance = new (NotNull, allocateCell<JSMap>(vm.heap)) JSMap(vm, structure);
    instance->finishCreation(globalObject, vm, this);
    return instance;
}

bool JSMap::isIteratorProtocolFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject->isMapPrototypeIteratorProtocolFastAndNonObservable())
        return false;

    VM& vm = globalObject->vm();
    Structure* structure = this->structure(vm);
    // This is the fast case. Many maps will be an original map.
    if (structure == globalObject->mapStructure())
        return true;

    if (getPrototypeDirect(vm) != globalObject->mapPrototype())
        return false;

    if (getDirectOffset(vm, vm.propertyNames->iteratorSymbol) != invalidOffset)
        return false;

    return true;
}

bool JSMap::canCloneFastAndNonObservable(Structure* structure)
{
    auto setFastAndNonObservable = [&] (Structure* structure) {
        JSGlobalObject* globalObject = structure->globalObject();
        if (!globalObject->isMapPrototypeSetFastAndNonObservable())
            return false;

        if (structure->hasPolyProto())
            return false;

        if (structure->storedPrototype() != globalObject->mapPrototype())
            return false;

        return true;
    };

    return isIteratorProtocolFastAndNonObservable() && setFastAndNonObservable(structure);
}

}
