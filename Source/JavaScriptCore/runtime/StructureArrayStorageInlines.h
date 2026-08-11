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

// This is a lightweight alternative to StructureInlines.h for files that
// need array storage and indexed access related Structure inline methods.
// It avoids the heavy transitive includes that StructureInlines.h pulls in.

#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/Structure.h>

namespace JSC {

inline bool Structure::mayInterceptIndexedAccesses() const
{
    if (indexingModeIncludingHistory() & MayHaveIndexedAccessors)
        return true;

    // Consider a scenario where object O (of global G1)'s prototype is set to A
    // (of global G2), and G2 is already having a bad time. If an object B with
    // indexed accessors is then set as the prototype of A:
    //      O -> A -> B
    // Then, O should be converted to SlowPutArrayStorage (because it now has an
    // object with indexed accessors in its prototype chain). But it won't be
    // converted because this conversion is done by JSGlobalObject::haveAbadTime(),
    // but G2 is already having a bad time. We solve this by conservatively
    // treating A as potentially having indexed accessors if its global is already
    // having a bad time. Hence, when A is set as O's prototype, O will be
    // converted to SlowPutArrayStorage.

    JSGlobalObject* globalObject = this->realm();
    if (!globalObject)
        return false;
    return globalObject->isHavingABadTime();
}

inline bool Structure::holesMustForwardToPrototype(JSObject* base) const
{
    ASSERT(base->structure() == this);
    if (typeInfo().type() == ArrayType) {
        JSGlobalObject* globalObject = this->realm();
        if (globalObject->isOriginalArrayStructure(const_cast<Structure*>(this)) && globalObject->arrayPrototypeChainIsSane()) [[likely]]
            return false;
    }

    if (this->mayInterceptIndexedAccesses())
        return true;

    return holesMustForwardToPrototypeSlow(base);
}

} // namespace JSC
