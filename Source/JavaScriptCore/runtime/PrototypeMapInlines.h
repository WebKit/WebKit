/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "PrototypeMap.h"
#include "WeakGCMapInlines.h"

namespace JSC {

ALWAYS_INLINE TriState PrototypeMap::isPrototype(JSObject* object) const
{
    if (!m_prototypes.contains(object))
        return FalseTriState;

    // We know that 'object' was used as a prototype at one time, so be
    // conservative and say that it might still be so. (It would be expensive
    // to find out for sure, and we don't know of any cases where being precise
    // would improve performance.)
    return MixedTriState;
}

ALWAYS_INLINE void PrototypeMap::addPrototype(JSObject* object)
{
    auto addResult = m_prototypes.add(object, Weak<JSObject>());
    if (addResult.isNewEntry)
        addResult.iterator->value = Weak<JSObject>(object);
    else
        ASSERT(addResult.iterator->value.get() == object);

    // Note that this method makes the somewhat odd decision to not check if this
    // object currently has indexed accessors. We could do that check here, and if
    // indexed accessors were found, we could tell the global object to have a bad
    // time. But we avoid this, to allow the following to be always fast:
    //
    // 1) Create an object.
    // 2) Give it a setter or read-only property that happens to have a numeric name.
    // 3) Allocate objects that use this object as a prototype.
    //
    // This avoids anyone having a bad time. Even if the instance objects end up
    // having indexed storage, the creation of indexed storage leads to a prototype
    // chain walk that detects the presence of indexed setters and then does the
    // right thing. As a result, having a bad time only happens if you add an
    // indexed setter (or getter, or read-only field) to an object that is already
    // used as a prototype.
}

} // namespace JSC

