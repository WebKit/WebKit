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

MemoryObjectType GenericMemoryTypes::Other = "Other";

MemoryObjectType WebCoreMemoryTypes::DOM = "DOM";
MemoryObjectType WebCoreMemoryTypes::CSS = "CSS";
MemoryObjectType WebCoreMemoryTypes::Binding = "Binding";
MemoryObjectType WebCoreMemoryTypes::Loader = "Loader";
MemoryObjectType WebCoreMemoryTypes::MemoryCacheStructures = "MemoryCacheStructures";
MemoryObjectType WebCoreMemoryTypes::CachedResource = "CachedResource";
MemoryObjectType WebCoreMemoryTypes::CachedResourceCSS = "CachedResourceCSS";
MemoryObjectType WebCoreMemoryTypes::CachedResourceFont = "CachedResourceFont";
MemoryObjectType WebCoreMemoryTypes::CachedResourceImage = "CachedResourceImage";
MemoryObjectType WebCoreMemoryTypes::CachedResourceScript = "CachedResourceScript";
MemoryObjectType WebCoreMemoryTypes::CachedResourceSVG = "CachedResourceSVG";
MemoryObjectType WebCoreMemoryTypes::CachedResourceShader = "CachedResourceShader";
MemoryObjectType WebCoreMemoryTypes::CachedResourceXSLT = "CachedResourceXSLT";

template<> void MemoryInstrumentationTraits::addInstrumentedObject<String>(MemoryInstrumentation* instrumentation, const String* const& string, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    MemoryInstrumentationTraits::addInstrumentedObject<const String>(instrumentation, string, ownerObjectType, owningType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<const String>(MemoryInstrumentation* instrumentation, const String* const& string, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (!string || instrumentation->visited(string))
        return;
    if (owningType == byPointer)
        instrumentation->countObjectSize(ownerObjectType, sizeof(String));
    instrumentation->addInstrumentedObject(string->impl(), ownerObjectType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<StringImpl>(MemoryInstrumentation* instrumentation, const StringImpl* const& stringImpl, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    MemoryInstrumentationTraits::addInstrumentedObject<const StringImpl>(instrumentation, stringImpl, ownerObjectType, owningType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<const StringImpl>(MemoryInstrumentation* instrumentation, const StringImpl* const& stringImpl, MemoryObjectType ownerObjectType, MemoryOwningType)
{
    if (!stringImpl || instrumentation->visited(stringImpl))
        return;
    instrumentation->countObjectSize(ownerObjectType, stringImpl->sizeInBytes());
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<KURL>(MemoryInstrumentation* instrumentation, const KURL* const& url, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    MemoryInstrumentationTraits::addInstrumentedObject<const KURL>(instrumentation, url, ownerObjectType, owningType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<const KURL>(MemoryInstrumentation* instrumentation, const KURL* const& url, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    if (!url || instrumentation->visited(url))
        return;
    if (owningType == byPointer)
        instrumentation->countObjectSize(ownerObjectType, sizeof(KURL));
    instrumentation->addInstrumentedObject(url->string(), ownerObjectType);
    if (url->innerURL())
        instrumentation->addInstrumentedObject(url->innerURL(), ownerObjectType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<AtomicString>(MemoryInstrumentation* instrumentation, const AtomicString* const& atomicString, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    MemoryInstrumentationTraits::addInstrumentedObject<const AtomicString>(instrumentation, atomicString, ownerObjectType, owningType);
}

template<> void MemoryInstrumentationTraits::addInstrumentedObject<const AtomicString>(MemoryInstrumentation* instrumentation, const AtomicString* const& atomicString, MemoryObjectType ownerObjectType, MemoryOwningType owningType)
{
    MemoryInstrumentationTraits::addInstrumentedObject<const String>(instrumentation, reinterpret_cast<const String* const>(atomicString), ownerObjectType, owningType);
}

} // namespace WebCore
