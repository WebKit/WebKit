/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextFlags.h"

#include <functional>
#include <numeric>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior::Behavior behavior)
{
    switch (behavior) {
    case ExpansionBehavior::Behavior::Forbid: ts << "forbid"; break;
    case ExpansionBehavior::Behavior::Allow: ts << "allow"; break;
    case ExpansionBehavior::Behavior::Force: ts << "force"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, ExpansionBehavior expansionBehavior)
{
    ts << expansionBehavior.left;
    ts << ' ';
    ts << expansionBehavior.right;
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, Kerning kerning)
{
    switch (kerning) {
    case Kerning::Auto: ts << "auto"; break;
    case Kerning::Normal: ts << "normal"; break;
    case Kerning::NoShift: ts << "no-shift"; break;
    }
    return ts;
}

template <typename T> using BinOp = std::function<T(T, T)>;

template <typename T>
T reduce(const std::vector<T>& vec, const BinOp<T>& binOp, std::optional<T> initial = { })
{
    if (vec.empty())
        return initial ? *initial : T { };

    auto accumulator = initial ? *initial : vec[0];
    auto start = initial ? 0 : 1;
    for (size_t i = start ; i < vec.size() ; i++)
        accumulator = binOp(accumulator, vec[i]);

    return accumulator;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantAlternates alternates)
{
    if (alternates.isNormal())
        ts << "normal";
    else {
        auto values = alternates.values();
        std::vector<String> result;
        if (values.stylistic)
            result.push_back("stylistic(" + *values.stylistic + ")");
        if (values.historicalForms)
            result.push_back("historical-forms"_s);
        if (values.styleset)
            result.push_back("styleset(" + *values.styleset + ")");
        if (values.characterVariant)
            result.push_back("character-variant(" + *values.characterVariant + ")");
        if (values.swash)
            result.push_back("swash(" + *values.swash + ")");
        if (values.ornaments)
            result.push_back("ornaments(" + *values.ornaments + ")");
        if (values.annotation)
            result.push_back("annotation(" + *values.annotation + ")");
        
        BinOp<String> addWithSpace = [](auto a, auto b) { 
            return a + " " + b;
        };
        ts << reduce(result, addWithSpace);
    }
    return ts;
}

void add(Hasher& hasher, const FontVariantAlternatesValues& key)
{
    add(hasher, key.historicalForms, key.stylistic, key.styleset, key.characterVariant, key.swash, key.ornaments, key.annotation);
}

void add(Hasher& hasher, const FontVariantAlternates& key)
{
    add(hasher, key.m_val.index());
    if (key.isNormal())
        add(hasher, 0);
    else
        add(hasher, key.values());
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantPosition position)
{
    switch (position) {
    case FontVariantPosition::Normal: ts << "normal"; break;
    case FontVariantPosition::Subscript: ts << "subscript"; break;
    case FontVariantPosition::Superscript: ts << "superscript"; break;
    }
    return ts;
}

WTF::TextStream& operator<<(TextStream& ts, FontVariantCaps caps)
{
    switch (caps) {
    case FontVariantCaps::Normal: ts << "normal"; break;
    case FontVariantCaps::Small: ts << "small"; break;
    case FontVariantCaps::AllSmall: ts << "all-small"; break;
    case FontVariantCaps::Petite: ts << "petite"; break;
    case FontVariantCaps::AllPetite: ts << "all-petite"; break;
    case FontVariantCaps::Unicase: ts << "unicase"; break;
    case FontVariantCaps::Titling: ts << "titling"; break;
    }
    return ts;
}

} // namespace WebCore
