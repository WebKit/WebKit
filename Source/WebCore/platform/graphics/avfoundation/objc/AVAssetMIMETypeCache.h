/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include "MIMETypeCache.h"

namespace WebCore {

class ContentType;

class WEBCORE_EXPORT AVAssetMIMETypeCache final : public MIMETypeCache {
public:
    static AVAssetMIMETypeCache& singleton();

    using CacheMIMETypesCallback = std::function<void(const Vector<String>&)>;
    void setCacheMIMETypesCallback(CacheMIMETypesCallback&& callback) { m_cacheTypeCallback = WTFMove(callback); }

    bool isAvailable() const final;

    void addSupportedTypes(const Vector<String>&);

private:
    friend NeverDestroyed<AVAssetMIMETypeCache>;
    AVAssetMIMETypeCache() = default;

    bool isStaticContainerType(StringView) final;
    bool isUnsupportedContainerType(const String&) final;
    bool canDecodeExtendedType(const ContentType&) final;
    void initializeCache(HashSet<String, ASCIICaseInsensitiveHash>&) final;

    CacheMIMETypesCallback m_cacheTypeCallback;
};

}

#endif
