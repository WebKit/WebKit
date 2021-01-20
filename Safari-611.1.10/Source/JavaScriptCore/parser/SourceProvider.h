/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CachedBytecode.h"
#include "CodeSpecializationKind.h"
#include "SourceOrigin.h"
#include <wtf/RefCounted.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class SourceCode;
class UnlinkedFunctionExecutable;
class UnlinkedFunctionCodeBlock;

    enum class SourceProviderSourceType : uint8_t {
        Program,
        Module,
        WebAssembly,
    };

    using BytecodeCacheGenerator = Function<RefPtr<CachedBytecode>()>;

    class SourceProvider : public RefCounted<SourceProvider> {
    public:
        static const intptr_t nullID = 1;
        
        JS_EXPORT_PRIVATE SourceProvider(const SourceOrigin&, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType);

        JS_EXPORT_PRIVATE virtual ~SourceProvider();

        virtual unsigned hash() const = 0;
        virtual StringView source() const = 0;
        virtual RefPtr<CachedBytecode> cachedBytecode() const { return nullptr; }
        virtual void cacheBytecode(const BytecodeCacheGenerator&) const { }
        virtual void updateCache(const UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, const UnlinkedFunctionCodeBlock*) const { }
        virtual void commitCachedBytecode() const { }

        StringView getRange(int start, int end) const
        {
            return source().substring(start, end - start);
        }

        const SourceOrigin& sourceOrigin() const { return m_sourceOrigin; }

        // This is NOT the path that should be used for computing relative paths from a script. Use SourceOrigin's URL for that, the values may or may not be the same...
        const String& sourceURL() const { return m_sourceURL; }
        const String& sourceURLDirective() const { return m_sourceURLDirective; }
        const String& sourceMappingURLDirective() const { return m_sourceMappingURLDirective; }

        TextPosition startPosition() const { return m_startPosition; }
        SourceProviderSourceType sourceType() const { return m_sourceType; }

        intptr_t asID()
        {
            if (!m_id)
                getID();
            return m_id;
        }

        void setSourceURLDirective(const String& sourceURLDirective) { m_sourceURLDirective = sourceURLDirective; }
        void setSourceMappingURLDirective(const String& sourceMappingURLDirective) { m_sourceMappingURLDirective = sourceMappingURLDirective; }

    private:
        JS_EXPORT_PRIVATE void getID();

        SourceProviderSourceType m_sourceType;
        SourceOrigin m_sourceOrigin;
        String m_sourceURL;
        String m_sourceURLDirective;
        String m_sourceMappingURLDirective;
        TextPosition m_startPosition;
        uintptr_t m_id { 0 };
    };

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringSourceProvider);
    class StringSourceProvider : public SourceProvider {
        WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StringSourceProvider);
    public:
        static Ref<StringSourceProvider> create(const String& source, const SourceOrigin& sourceOrigin, String sourceURL, const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
        {
            return adoptRef(*new StringSourceProvider(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType));
        }
        
        unsigned hash() const override
        {
            return m_source.get().hash();
        }

        StringView source() const override
        {
            return m_source.get();
        }

    protected:
        StringSourceProvider(const String& source, const SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
            : SourceProvider(sourceOrigin, WTFMove(sourceURL), startPosition, sourceType)
            , m_source(source.isNull() ? *StringImpl::empty() : *source.impl())
        {
        }

    private:
        Ref<StringImpl> m_source;
    };

#if ENABLE(WEBASSEMBLY)
    class WebAssemblySourceProvider final : public SourceProvider {
    public:
        static Ref<WebAssemblySourceProvider> create(Vector<uint8_t>&& data, const SourceOrigin& sourceOrigin, String sourceURL)
        {
            return adoptRef(*new WebAssemblySourceProvider(WTFMove(data), sourceOrigin, WTFMove(sourceURL)));
        }

        unsigned hash() const final
        {
            return m_source.impl()->hash();
        }

        StringView source() const final
        {
            return m_source;
        }

        const Vector<uint8_t>& data() const
        {
            return m_data;
        }

    private:
        WebAssemblySourceProvider(Vector<uint8_t>&& data, const SourceOrigin& sourceOrigin, String&& sourceURL)
            : SourceProvider(sourceOrigin, WTFMove(sourceURL), TextPosition(), SourceProviderSourceType::WebAssembly)
            , m_source("[WebAssembly source]")
            , m_data(WTFMove(data))
        {
        }

        String m_source;
        Vector<uint8_t> m_data;
    };
#endif

} // namespace JSC
