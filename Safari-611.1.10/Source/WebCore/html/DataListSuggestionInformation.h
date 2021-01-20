/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "IntRect.h"
#include <wtf/EnumTraits.h>
#include <wtf/Vector.h>

#if ENABLE(DATALIST_ELEMENT)

namespace WebCore {

enum class DataListSuggestionActivationType : uint8_t {
    ControlClicked,
    IndicatorClicked,
    TextChanged,
};

struct DataListSuggestion {
    String value;
    String label;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<DataListSuggestion> decode(Decoder&);
};

template<class Encoder>
void DataListSuggestion::encode(Encoder& encoder) const
{
    encoder << value;
    encoder << label;
}

template<class Decoder>
Optional<DataListSuggestion> DataListSuggestion::decode(Decoder& decoder)
{
    Optional<String> value;
    decoder >> value;
    if (!value)
        return WTF::nullopt;

    Optional<String> label;
    decoder >> label;
    if (!label)
        return WTF::nullopt;

    return {{ *value, *label }};
}

struct DataListSuggestionInformation {
    DataListSuggestionActivationType activationType;
    Vector<DataListSuggestion> suggestions;
    IntRect elementRect;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<DataListSuggestionInformation> decode(Decoder&);
};

template<class Encoder>
void DataListSuggestionInformation::encode(Encoder& encoder) const
{
    encoder << activationType;
    encoder << suggestions;
    encoder << elementRect;
}

template<class Decoder>
Optional<DataListSuggestionInformation> DataListSuggestionInformation::decode(Decoder& decoder)
{
    DataListSuggestionActivationType activationType;
    if (!decoder.decode(activationType))
        return WTF::nullopt;

    Optional<Vector<DataListSuggestion>> suggestions;
    decoder >> suggestions;
    if (!suggestions)
        return WTF::nullopt;

    Optional<IntRect> elementRect;
    decoder >> elementRect;
    if (!elementRect)
        return WTF::nullopt;

    return {{ activationType, *suggestions, *elementRect }};
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DataListSuggestionActivationType> {
    using values = EnumValues<
        WebCore::DataListSuggestionActivationType,
        WebCore::DataListSuggestionActivationType::ControlClicked,
        WebCore::DataListSuggestionActivationType::IndicatorClicked,
        WebCore::DataListSuggestionActivationType::TextChanged
    >;
};

} // namespace WTF

#endif // ENABLE(DATALIST_ELEMENT)
