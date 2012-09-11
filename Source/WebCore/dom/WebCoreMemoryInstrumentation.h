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

#ifndef WebCoreMemoryInstrumentation_h
#define WebCoreMemoryInstrumentation_h

#include <wtf/Forward.h>
#include <wtf/MemoryInstrumentation.h>

namespace WebCore {

class KURL;

// Explicit specializations for some types.
template<> void MemoryInstrumentationTraits::addInstrumentedObject<KURL>(MemoryInstrumentation*, const KURL* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addInstrumentedObject<const KURL>(MemoryInstrumentation*, const KURL* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addInstrumentedObject<String>(MemoryInstrumentation*, const String* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addInstrumentedObject<const String>(MemoryInstrumentation*, const String* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addInstrumentedObject<StringImpl>(MemoryInstrumentation*, const StringImpl* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addInstrumentedObject<const StringImpl>(MemoryInstrumentation*, const StringImpl* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addInstrumentedObject<AtomicString>(MemoryInstrumentation*, const AtomicString* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addInstrumentedObject<const AtomicString>(MemoryInstrumentation*, const AtomicString* const&, MemoryObjectType, MemoryOwningType);


// Link time guards with no body.
template<> void MemoryInstrumentationTraits::addObject<KURL>(MemoryInstrumentation*, const KURL* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addObject<const KURL>(MemoryInstrumentation*, const KURL* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addObject<String>(MemoryInstrumentation*, const String* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addObject<const String>(MemoryInstrumentation*, const String* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addObject<StringImpl>(MemoryInstrumentation*, const StringImpl* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addObject<const StringImpl>(MemoryInstrumentation*, const StringImpl* const&, MemoryObjectType, MemoryOwningType);

template<> void MemoryInstrumentationTraits::addObject<AtomicString>(MemoryInstrumentation*, const AtomicString* const&, MemoryObjectType, MemoryOwningType);
template<> void MemoryInstrumentationTraits::addObject<const AtomicString>(MemoryInstrumentation*, const AtomicString* const&, MemoryObjectType, MemoryOwningType);

class WebCoreMemoryTypes {
public:
    static MemoryObjectType Page;
    static MemoryObjectType DOM;
    static MemoryObjectType CSS;
    static MemoryObjectType Binding;
    static MemoryObjectType Loader;

    static MemoryObjectType MemoryCache;
    static MemoryObjectType MemoryCacheStructures;
    static MemoryObjectType CachedResource;
    static MemoryObjectType CachedResourceRaw;
    static MemoryObjectType CachedResourceCSS;
    static MemoryObjectType CachedResourceFont;
    static MemoryObjectType CachedResourceImage;
    static MemoryObjectType CachedResourceScript;
    static MemoryObjectType CachedResourceSVG;
    static MemoryObjectType CachedResourceShader;
    static MemoryObjectType CachedResourceXSLT;
};

} // namespace WebCore

#endif // !defined(WebCoreMemoryInstrumentation_h)
