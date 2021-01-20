/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include "JSArrayBufferPrototype.h"
#include "JSGlobalObject.h"

namespace JSC  {

namespace JSArrayBufferPrototypeInternal {
static constexpr bool verbose = false;
};

ALWAYS_INLINE bool speciesWatchpointIsValid(VM& vm, JSObject* thisObject, ArrayBufferSharingMode mode)
{
    JSGlobalObject* globalObject = thisObject->globalObject(vm);
    auto* prototype = globalObject->arrayBufferPrototype(mode);

    if (globalObject->arrayBufferSpeciesWatchpointSet(mode).stateOnJSThread() == ClearWatchpoint) {
        dataLogLnIf(JSArrayBufferPrototypeInternal::verbose, "Initializing ArrayBuffer species watchpoints for ArrayBuffer.prototype: ", pointerDump(prototype), " with structure: ", pointerDump(prototype->structure(vm)), "\nand ArrayBuffer: ", pointerDump(globalObject->arrayBufferConstructor(mode)), " with structure: ", pointerDump(globalObject->arrayBufferConstructor(mode)->structure(vm)));
        globalObject->tryInstallArrayBufferSpeciesWatchpoint(mode);
        ASSERT(globalObject->arrayBufferSpeciesWatchpointSet(mode).stateOnJSThread() != ClearWatchpoint);
    }

    return !thisObject->hasCustomProperties(vm)
        && prototype == thisObject->getPrototypeDirect(vm)
        && globalObject->arrayBufferSpeciesWatchpointSet(mode).stateOnJSThread() == IsWatched;
}

ALWAYS_INLINE Optional<JSValue> arrayBufferSpeciesConstructor(JSGlobalObject* globalObject, JSArrayBuffer* thisObject, ArrayBufferSharingMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isValid = speciesWatchpointIsValid(vm, thisObject, mode);
    scope.assertNoException();
    if (LIKELY(isValid))
        return WTF::nullopt;

    RELEASE_AND_RETURN(scope, arrayBufferSpeciesConstructorSlow(globalObject, thisObject, mode));
}

} // namespace JSC
