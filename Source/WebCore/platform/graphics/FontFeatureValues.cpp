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
#include "FontFeatureValues.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

void add(Hasher& hasher, const FontFeatureValues& fontFeatureValues)
{
    auto hashTags = [&hasher](const auto& tags) {
        add(hasher, tags.isEmpty());
        for (const auto& tag : tags)
            add(hasher, tag.key, tag.value);
    };
    hashTags(fontFeatureValues.m_styleset);
    hashTags(fontFeatureValues.m_stylistic);
    hashTags(fontFeatureValues.m_characterVariant);
    hashTags(fontFeatureValues.m_swash);
    hashTags(fontFeatureValues.m_ornaments);
    hashTags(fontFeatureValues.m_annotation);
}

void FontFeatureValues::updateOrInsert(const FontFeatureValues& other)
{
    if (this == &other)
        return;
    
    auto updateOrInsertTags = [](auto& into, const auto& tags) {
        for (const auto& tag : tags)
            into.set(tag.key, tag.value);
    };
    updateOrInsertTags(m_styleset, other.styleset());
    updateOrInsertTags(m_stylistic, other.stylistic());
    updateOrInsertTags(m_characterVariant, other.characterVariant());
    updateOrInsertTags(m_swash, other.swash());
    updateOrInsertTags(m_ornaments, other.ornaments());
    updateOrInsertTags(m_annotation, other.annotation());
}

void FontFeatureValues::updateOrInsertForType(FontFeatureValuesType type, const Vector<FontFeatureValuesTag>& tags)
{
    auto updateOrInsertTags = [](auto& into, const Vector<FontFeatureValuesTag>& tags) {
        for (const FontFeatureValuesTag& tag : tags)
            into.set(tag.first, tag.second);
    };
    switch (type) {
    case FontFeatureValuesType::Styleset:
        updateOrInsertTags(m_styleset, tags);
        break;
    case FontFeatureValuesType::Stylistic:
        updateOrInsertTags(m_stylistic, tags);
        break;
    case FontFeatureValuesType::CharacterVariant:
        updateOrInsertTags(m_characterVariant, tags);
        break;
    case FontFeatureValuesType::Swash:
        updateOrInsertTags(m_swash, tags);
        break;
    case FontFeatureValuesType::Ornaments:
        updateOrInsertTags(m_ornaments, tags);
        break;
    case FontFeatureValuesType::Annotation:
        updateOrInsertTags(m_annotation, tags);
        break;
    }
    
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const FontFeatureValues& fontFeatureValues)
{
    auto printTags = [&ts](const auto& name, const auto& tags) {
        if (tags.isEmpty())
            return;

        ts << "{" << name << ": ";
        for (const auto& tag : tags)
            ts << "{" << tag.key << ":" << tag.value << "} ";
        ts << "}";
    };
    printTags("styleset", fontFeatureValues.m_styleset);
    printTags("stylistic", fontFeatureValues.m_stylistic);
    printTags("characterVariant", fontFeatureValues.m_characterVariant);
    printTags("swash", fontFeatureValues.m_swash);
    printTags("ornaments", fontFeatureValues.m_ornaments);
    printTags("annotation", fontFeatureValues.m_annotation);
    return ts;  
}

} // namespace WebCore
