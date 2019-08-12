/*
 * Copyright (C) 2018 mce sys Ltd. All rights reserved.
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

#include <wtf/Function.h>
#include <wtf/text/StringImpl.h>

namespace WTF {

class ExternalStringImpl;

using ExternalStringImplFreeFunction = Function<void(ExternalStringImpl*, void*, unsigned)>;

class ExternalStringImpl final : public StringImpl {
public:
    WTF_EXPORT_PRIVATE static Ref<ExternalStringImpl> create(const LChar* characters, unsigned length, ExternalStringImplFreeFunction&&);
    WTF_EXPORT_PRIVATE static Ref<ExternalStringImpl> create(const UChar* characters, unsigned length, ExternalStringImplFreeFunction&&);

private:
    friend class StringImpl;

    ExternalStringImpl(const LChar* characters, unsigned length, ExternalStringImplFreeFunction&&);
    ExternalStringImpl(const UChar* characters, unsigned length, ExternalStringImplFreeFunction&&);

    ALWAYS_INLINE void freeExternalBuffer(void* buffer, unsigned bufferSize)
    {
        m_free(this, buffer, bufferSize);
    }

    ExternalStringImplFreeFunction m_free;
};

} // namespace WTF

using WTF::ExternalStringImpl;
