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

#include <WebCore/Color.h>
#include <WebCore/Gradient.h>
#include <wtf/HashFunctions.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct FontPaletteIndex {
    enum class Type : uint8_t;

    FontPaletteIndex() = default;

    FontPaletteIndex(Type type)
        : type(type)
    {
        ASSERT(type == Type::Light || type == Type::Dark);
    }

    FontPaletteIndex(unsigned integer)
        : type(Type::Integer)
        , integer(integer)
    {
    }

    operator bool() const
    {
        return type != Type::Integer || integer;
    }

    bool operator==(const FontPaletteIndex& other) const
    {
        if (type != other.type)
            return false;
        if (type == Type::Integer)
            return integer == other.integer;
        return true;
    }

    enum class Type : uint8_t {
        Light,
        Dark,
        Integer
    };
    Type type { Type::Integer };

    unsigned integer { 0 };
};

inline void add(Hasher& hasher, const FontPaletteIndex& paletteIndex)
{
    add(hasher, paletteIndex.type);
    if (paletteIndex.type == FontPaletteIndex::Type::Integer)
        add(hasher, paletteIndex.integer);
}

class FontPaletteValues {
public:
    using OverriddenColor = std::pair<unsigned, Color>;

    struct CustomPalette {
        AtomString identifier;
        std::optional<FontPaletteIndex> basePalette;
        Vector<OverriddenColor> overrideColors;

        bool operator==(const CustomPalette&) const = default;
    };

    FontPaletteValues() = default;

    explicit FontPaletteValues(Vector<CustomPalette>&& customPalettes)
        : m_customPalettes(WTF::move(customPalettes))
    {
    }

    const Vector<CustomPalette>& customPalettes() const
    {
        return m_customPalettes;
    }

    const CustomPalette* customPalette(const AtomString& identifier) const
    {
        for (auto& palette : m_customPalettes) {
            if (palette.identifier == identifier)
                return &palette;
        }
        return nullptr;
    }

    operator bool() const
    {
        return !m_customPalettes.isEmpty();
    }

    bool operator==(const FontPaletteValues& other) const
    {
        return m_customPalettes == other.m_customPalettes;
    }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

private:
    Vector<CustomPalette> m_customPalettes;
};

inline void add(Hasher& hasher, const FontPaletteValues::CustomPalette& customPalette)
{
    add(hasher, customPalette.identifier, customPalette.basePalette, customPalette.overrideColors);
}

inline void add(Hasher& hasher, const FontPaletteValues& fontPaletteValues)
{
    add(hasher, fontPaletteValues.customPalettes());
}

} // namespace WebCore
