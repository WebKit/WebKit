/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "ArgumentCodersCocoa.h"
#include <wtf/RetainPtr.h>
#include <wtf/cf/VectorCF.h>

namespace WebKit {

class CoreIPCCFCharacterSet {
public:
    CoreIPCCFCharacterSet(CFCharacterSetRef characterSet)
        : m_cfCharacterSetData(dataFromCharacterSet(characterSet))
    {
        ASSERT(m_cfCharacterSetData);
    }

    CoreIPCCFCharacterSet(RetainPtr<CFDataRef> characterSetData)
        : m_cfCharacterSetData(WTFMove(characterSetData))
    {
        ASSERT(m_cfCharacterSetData);
    }

    CoreIPCCFCharacterSet(std::span<const uint8_t> data)
        : m_cfCharacterSetData(adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size())))
    {
    }

    RetainPtr<CFCharacterSetRef> toCF() const
    {
        return adoptCF(CFCharacterSetCreateWithBitmapRepresentation(nullptr, m_cfCharacterSetData.get()));
    }

    std::span<const uint8_t> dataReference() const
    {
        ASSERT(m_cfCharacterSetData);
        return span(m_cfCharacterSetData.get());
    }

private:
    RetainPtr<CFDataRef> dataFromCharacterSet(CFCharacterSetRef characterSet)
    {
        ASSERT(characterSet);
        auto data = adoptCF(CFCharacterSetCreateBitmapRepresentation(nullptr, characterSet));
        ASSERT(data);
        return data;
    }

    RetainPtr<CFDataRef> m_cfCharacterSetData;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
