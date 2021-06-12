/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_ELEMENT)

#include "SharedBuffer.h"
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Model final : public RefCounted<Model> {
public:
    WEBCORE_EXPORT static Ref<Model> create(Ref<SharedBuffer>);
    WEBCORE_EXPORT ~Model();

    Ref<SharedBuffer> data() const { return m_data; }
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static RefPtr<Model> decode(Decoder&);

private:
    explicit Model(Ref<SharedBuffer>);

    Ref<SharedBuffer> m_data;
};

template<class Encoder>
void Model::encode(Encoder& encoder) const
{
    encoder << static_cast<size_t>(m_data->size());
    encoder.encodeFixedLengthData(m_data->data(), m_data->size(), 1);
}

template<class Decoder>
RefPtr<Model> Model::decode(Decoder& decoder)
{
    std::optional<size_t> length;
    decoder >> length;
    if (!length)
        return nullptr;

    if (!decoder.template bufferIsLargeEnoughToContain<uint8_t>(length.value())) {
        decoder.markInvalid();
        return nullptr;
    }

    Vector<uint8_t> data;
    data.grow(*length);
    if (!decoder.decodeFixedLengthData(data.data(), data.size(), 1))
        return nullptr;

    return Model::create(SharedBuffer::create(WTFMove(data)));
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Model&);

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT)
