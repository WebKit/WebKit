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
#include <wtf/Forward.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class GlyphBuffer;
struct TextAutospace;
class TextRun;
enum class FontWidthVariant : uint8_t;

struct TextSpacingTrim {
    enum class TrimType : bool {
        SpaceAll = 0, // equivalent to None in text-spacing shorthand
        Auto
    };

    bool isAuto() const { return m_trim == TrimType::Auto; }
    bool isSpaceAll() const { return m_trim == TrimType::SpaceAll; }
    friend bool operator==(const TextSpacingTrim&, const TextSpacingTrim&) = default;
    TrimType m_trim { TrimType::SpaceAll };
};

inline WTF::TextStream& operator<<(WTF::TextStream& ts, const TextSpacingTrim& value)
{
    // FIXME: add remaining values;
    switch (value.m_trim) {
    case TextSpacingTrim::TrimType::Auto:
        return ts << "auto";
    case TextSpacingTrim::TrimType::SpaceAll:
        return ts << "space-all";
    }
    return ts;
}

struct TextAutospace {
    enum class TextAutospaceType : bool {
        NoAutospace = 0, // equivalent to None in text-spacing shorthand
        Auto
    };

    bool isAuto() const { return m_autoSpace == TextAutospaceType::Auto; }
    bool isNoAutospace() const { return m_autoSpace == TextAutospaceType::NoAutospace; }
    friend bool operator==(const TextAutospace&, const TextAutospace&) = default;
    TextAutospaceType m_autoSpace { TextAutospaceType::NoAutospace };
};

inline WTF::TextStream& operator<<(WTF::TextStream& ts, const TextAutospace& value)
{
    // FIXME: add remaining values;
    switch (value.m_autoSpace) {
    case TextAutospace::TextAutospaceType::Auto:
        return ts << "auto";
    case TextAutospace::TextAutospaceType::NoAutospace:
        return ts << "no-autospace";
    }
    return ts;
}

struct TextSpacingWidth {
    float leftWidth = 0.0;
    float rightWidth = 0.0;
};

enum class TextSpacingSupportedLanguage : uint8_t {
    NotSupported,
    Japanese,
    SimplifiedChinese,
    TraditionalChinese
};

struct TextSpacingEngineState {
    float leftoverWidth = 0.0;
    UChar32 firstCharacterOfNextRun = 0;
};

class TextSpacingEngine {
public:
    TextSpacingEngine(TextSpacingEngineState *state, GlyphBuffer& glyphBuffer, const TextRun& run)
        : m_state(state)
        , m_glyphBuffer(glyphBuffer)
        , m_run(run)
    { }

    TextSpacingWidth adjustTextSpacing(unsigned currentCharacterIndex, unsigned glyphIndex);

private:
    TextSpacingWidth processTextSpacingAndUpdateState(unsigned glyphBufferIndex, const TextSpacingWidth&);

    TextSpacingEngineState* m_state;
    GlyphBuffer& m_glyphBuffer;
    const TextRun& m_run;
};

TextSpacingWidth calculateCJKAdjustingSpacing(TextSpacingSupportedLanguage, UChar32, UChar32);

} // namespace WebCore
