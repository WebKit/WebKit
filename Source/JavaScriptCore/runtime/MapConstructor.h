/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "InternalFunction.h"

namespace JSC {

class MapPrototype;
class GetterSetter;

class MapConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;

    static MapConstructor* create(VM& vm, Structure* structure, MapPrototype* mapPrototype, GetterSetter* speciesSymbol)
    {
        MapConstructor* constructor = new (NotNull, allocateCell<MapConstructor>(vm)) MapConstructor(vm, structure);
        constructor->finishCreation(vm, mapPrototype, speciesSymbol);
        return constructor;
    }

    DECLARE_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    MapConstructor(VM&, Structure*);

    void finishCreation(VM&, MapPrototype*, GetterSetter* speciesSymbol);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(MapConstructor, InternalFunction);

JSC_DECLARE_HOST_FUNCTION(mapPrivateFuncMapBucketHead);
JSC_DECLARE_HOST_FUNCTION(mapPrivateFuncMapBucketNext);
JSC_DECLARE_HOST_FUNCTION(mapPrivateFuncMapBucketKey);
JSC_DECLARE_HOST_FUNCTION(mapPrivateFuncMapBucketValue);

} // namespace JSC
