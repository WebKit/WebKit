/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StringEntropyHelpers.h"

#include "MIMETypeRegistry.h"
#include <array>
#include <wtf/text/MakeString.h>

namespace WebCore::StringEntropyHelpers {

enum class Symbol : uint8_t {
    HexLower = 0,
    NonHexLower,
    HexUpper,
    NonHexUpper,
    Digit,
    Dash,
    Underscore,
    FullStop,
    OtherPunctuation,
    OtherCharacter,
    NumberOfSymbols // Must stay at the end.
};

static constexpr size_t numberOfSymbols = static_cast<size_t>(Symbol::NumberOfSymbols);

static Symbol symbol(UChar32 character)
{
    if (character >= 'a' && character <= 'f')
        return Symbol::HexLower;

    if (character >= 'g' && character <= 'z')
        return Symbol::NonHexLower;

    if (character >= 'A' && character <= 'F')
        return Symbol::HexUpper;

    if (character >= 'G' && character <= 'Z')
        return Symbol::NonHexUpper;

    if (character >= '0' && character <= '9')
        return Symbol::Digit;

    switch (character) {
    case '-':
        return Symbol::Dash;
    case '_':
        return Symbol::Underscore;
    case '.':
        return Symbol::FullStop;
    case '+':
    case '=':
    case '/':
    case '\\':
        return Symbol::OtherPunctuation;
    default:
        break;
    }

    return Symbol::OtherCharacter;
}

static constexpr std::array<uint8_t, numberOfSymbols * numberOfSymbols> quantizedBigramWeights { {
    153, 214, 100,  97, 116, 180, 199, 179, 119, 121, // HexLower
    209, 194,  86,  78, 106, 236, 203, 212, 123, 183, // NonHexLower
    135, 125,  75,  93,  92, 124, 122, 121,  17, 173, // HexUpper
    138, 100,  89,  80,  53, 145, 122, 117,  75, 114, // NonHexUpper
    111,  99, 114,  53, 126, 139, 155, 152,  86, 157, // Digit
    179, 235, 186, 185, 147, 144, 154, 164,  77, 156, // Dash
    199, 194, 160, 154, 167, 162, 174, 112, 127, 112, // Underscore
    173, 174, 210, 141, 162, 221, 104, 147, 167, 255, // FullStop
    134, 153,  66,  72, 128,  88,  50, 127,   0, 114, // OtherPunctuation
    158, 160, 112,  53, 167, 187, 131, 174, 182, 163, // OtherCharacter
} };

static double dequantize(uint8_t quantizedWeight)
{
    static constexpr double weightScale = 0.0273696267;
    static constexpr double weightZeroPoint = -4.0833584258;
    return quantizedWeight * weightScale + weightZeroPoint;
}

static double bigramWeight(Symbol first, Symbol second)
{
    auto tableOffset = numberOfSymbols * static_cast<uint8_t>(first) + static_cast<uint8_t>(second);
    return dequantize(quantizedBigramWeights[tableOffset]);
}

static double entropyScore(StringView text)
{
    auto textLength = text.length();
    if (textLength <= 1)
        return 0;

    double totalWeight = 0;
    std::optional<Symbol> previousSymbol;
    for (size_t i = 0; i < textLength; ++i) {
        auto character = text[i];
        auto currentSymbol = symbol(character);
        if (previousSymbol)
            totalWeight += bigramWeight(*previousSymbol, currentSymbol);
        previousSymbol = currentSymbol;
    }

    return totalWeight / textLength;
}

bool isProbablyHumanReadable(StringView text)
{
    static constexpr auto highEntropyThreshold = 40;
    static constexpr auto lowEntropyThreshold = 5;

    if (text.length() >= highEntropyThreshold)
        return false;

    if (text.length() <= lowEntropyThreshold)
        return true;

    return entropyScore(text) >= 0;
}

String lowEntropyLastPathComponent(const URL& url, const String& fallbackName, const String& mimeType)
{
    if (url.protocolIsData() || url.protocolIsBlob() || url.protocolIsJavaScript())
        return fallbackName;

    auto result = [&url, &fallbackName] -> String {
        auto component = url.lastPathComponent();
        if (isProbablyHumanReadable(component))
            return component.toString();

        auto fullStopIndex = component.reverseFind('.');
        if (fullStopIndex == notFound || fullStopIndex >= component.length() - 1)
            return fallbackName;

        auto extensionWithoutFullStop = component.right(component.length() - fullStopIndex - 1);
        auto firstNonAlphaNumericCharacterIndex = extensionWithoutFullStop.find([](char16_t character) {
            return !u_isalpha(character) && !u_isdigit(character);
        });

        if (firstNonAlphaNumericCharacterIndex != notFound)
            extensionWithoutFullStop = extensionWithoutFullStop.left(firstNonAlphaNumericCharacterIndex);

        if (extensionWithoutFullStop.isEmpty())
            return fallbackName;

        return makeString(fallbackName, '.', WTF::move(extensionWithoutFullStop));
    }();

    return MIMETypeRegistry::appendFileExtensionIfNecessary(WTF::move(result), mimeType);
}

URL removeHighEntropyComponents(const URL& url)
{
    if (url.protocolIs("mailto"_s) || url.protocolIs("tel"_s))
        return url;

    if (url.protocolIsData() || url.protocolIsBlob() || url.protocolIsJavaScript()) {
        URL urlPreservingProtocolOnly;
        urlPreservingProtocolOnly.setProtocol(url.protocol());
        return urlPreservingProtocolOnly;
    }

    auto newURL = url;

    StringBuilder newPath;
    bool removedAnyPathComponent = false;
    for (auto component : url.path().split('/')) {
        if (!isProbablyHumanReadable(component)) {
            removedAnyPathComponent = true;
            continue;
        }

        if (!newPath.isEmpty())
            newPath.append('/');

        newPath.append(component);
    }

    if (removedAnyPathComponent)
        newURL.setPath(newPath.toString());

    newURL.removeQueryAndFragmentIdentifier();
    return newURL;
}

} // namespace WebCore::StringEntropyHelpers
