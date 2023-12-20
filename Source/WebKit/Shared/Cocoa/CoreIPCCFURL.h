/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA)

#include <wtf/ArgumentCoder.h>
#include <wtf/URL.h>

namespace WebKit {

class CoreIPCCFURL {
public:
    CoreIPCCFURL(CFURLRef url)
        : m_cfURL(url)
    {
        RELEASE_ASSERT(url);
    }

    CoreIPCCFURL(RetainPtr<CFURLRef> url)
        : m_cfURL(WTFMove(url))
    {
        RELEASE_ASSERT(m_cfURL);
    }

    std::optional<CoreIPCCFURL> baseURL() const;
    Vector<uint8_t> bytes() const;

    RetainPtr<CFURLRef> createCFURL() const { return m_cfURL; }

    static std::optional<CoreIPCCFURL> createWithBaseURLAndBytes(std::optional<CoreIPCCFURL>&& baseURL, Vector<uint8_t>&& bytes);

private:
    RetainPtr<CFURLRef> m_cfURL;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
