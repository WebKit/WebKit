/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryInstrumentation.h"

#include "KURL.h"
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

void MemoryInstrumentation::addInstrumentedObjectImpl(const String* const& string, ObjectType objectType, OwningType owningType)
{
    if (!string || visited(string))
        return;
    if (owningType == byPointer)
        countObjectSize(objectType, sizeof(String));
    addInstrumentedObjectImpl(string->impl(), objectType, byPointer);
}

void MemoryInstrumentation::addInstrumentedObjectImpl(const StringImpl* const& stringImpl, ObjectType objectType, OwningType)
{
    if (!stringImpl || visited(stringImpl))
        return;
    countObjectSize(objectType, stringImpl->sizeInBytes());
}

void MemoryInstrumentation::addInstrumentedObjectImpl(const KURL* const& url, ObjectType objectType, OwningType owningType)
{
    if (!url || visited(url))
        return;
    if (owningType == byPointer)
        countObjectSize(objectType, sizeof(KURL));
    addInstrumentedObject(url->string(), objectType);
    if (url->innerURL())
        addInstrumentedObject(url->innerURL(), objectType);
}

void MemoryInstrumentation::addInstrumentedObjectImpl(const AtomicString* const& atomicString, ObjectType objectType, OwningType owningType)
{
    addInstrumentedObjectImpl(reinterpret_cast<const String* const>(atomicString), objectType, owningType);
}

} // namespace WebCore
