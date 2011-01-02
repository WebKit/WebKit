/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef SourceProvider_h
#define SourceProvider_h

#include "UString.h"
#include <wtf/RefCounted.h>

namespace JSC {

    class SourceProvider : public RefCounted<SourceProvider> {
    public:
        SourceProvider(const UString& url)
            : m_url(url)
            , m_validated(false)
        {
        }
        virtual ~SourceProvider() { }

        virtual UString getRange(int start, int end) const = 0;
        virtual const UChar* data() const = 0;
        virtual int length() const = 0;
        
        const UString& url() { return m_url; }
        intptr_t asID() { return reinterpret_cast<intptr_t>(this); }

        bool isValid() const { return m_validated; }
        void setValid() { m_validated = true; }

    private:
        UString m_url;
        bool m_validated;
    };

    class UStringSourceProvider : public SourceProvider {
    public:
        static PassRefPtr<UStringSourceProvider> create(const UString& source, const UString& url)
        {
            return adoptRef(new UStringSourceProvider(source, url));
        }

        UString getRange(int start, int end) const
        {
            return m_source.substringSharingImpl(start, end - start);
        }
        const UChar* data() const { return m_source.characters(); }
        int length() const { return m_source.length(); }

    private:
        UStringSourceProvider(const UString& source, const UString& url)
            : SourceProvider(url)
            , m_source(source)
        {
        }

        UString m_source;
    };
    
} // namespace JSC

#endif // SourceProvider_h
