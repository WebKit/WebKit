/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include <WebCore/SharedBuffer.h>
#include <wtf/URL.h>

namespace IPC {
template<typename T, typename> struct ArgumentCoder;
}

namespace WebCore {

class BlobPart {
private:
    friend struct IPC::ArgumentCoder<BlobPart, void>;
public:
    using VariantType = Variant<Vector<uint8_t>, Ref<SharedBuffer>, URL>;

    enum class Type : bool {
        Data,
        Blob
    };

    BlobPart()
        : m_dataOrURL(Vector<uint8_t> { })
    {
    }

    explicit BlobPart(Vector<uint8_t>&& data)
        : m_dataOrURL(WTFMove(data))
    {
    }

    explicit BlobPart(Ref<SharedBuffer>&& data)
        : m_dataOrURL(WTFMove(data))
    {
    }

    explicit BlobPart(const URL& url)
        : m_dataOrURL(url)
    {
    }

    Type type() const
    {
        return std::holds_alternative<URL>(m_dataOrURL) ? Type::Blob : Type::Data;
    }

    Vector<uint8_t> takeData()
    {
        auto blobPartVariant = std::exchange(m_dataOrURL, Vector<uint8_t> { });
        return switchOn(WTFMove(blobPartVariant), [](Ref<SharedBuffer>&& buffer) {
            return buffer->extractData();
        }, [](Vector<uint8_t>&& vector) {
            return WTFMove(vector);
        }, [](URL&&) {
            ASSERT_NOT_REACHED();
            return Vector<uint8_t> { };
        });
    }

    const URL& url() const
    {
        ASSERT(std::holds_alternative<URL>(m_dataOrURL));
        return std::get<URL>(m_dataOrURL);
    }

    void detachFromCurrentThread()
    {
        if (std::holds_alternative<URL>(m_dataOrURL))
            m_dataOrURL = std::get<URL>(m_dataOrURL).isolatedCopy();
    }

private:
    explicit BlobPart(VariantType&& dataOrURL)
        : m_dataOrURL(WTFMove(dataOrURL))
    {
    }

    VariantType m_dataOrURL;
};

} // namespace WebCore
