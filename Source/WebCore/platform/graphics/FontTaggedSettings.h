/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <array>
#include <wtf/HashTraits.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

typedef std::array<char, 4> FontTag;

inline FontTag fontFeatureTag(const char arr[4]) { return {{ arr[0], arr[1], arr[2], arr[3] }}; }

struct FourCharacterTagHash {
    static unsigned hash(const FontTag& characters) { return (characters[0] << 24) | (characters[1] << 16) | (characters[2] << 8) | characters[3]; }
    static bool equal(const FontTag& a, const FontTag& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FourCharacterTagHashTraits : WTF::GenericHashTraits<FontTag> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(FontTag& slot) { new (NotNull, std::addressof(slot)) FontTag({{ ff, ff, ff, ff }}); }
    static bool isDeletedValue(const FontTag& value) { return value == FontTag({{ ff, ff, ff, ff }}); }

private:
    const static char ff = static_cast<char>(0xFF);
};

template <typename T>
class FontTaggedSetting {
public:
    FontTaggedSetting() = delete;
    FontTaggedSetting(const FontTag&, T value);
    FontTaggedSetting(FontTag&&, T value);

    bool operator==(const FontTaggedSetting<T>& other) const;
    bool operator!=(const FontTaggedSetting<T>& other) const { return !(*this == other); }
    bool operator<(const FontTaggedSetting<T>& other) const;

    const FontTag& tag() const { return m_tag; }
    T value() const { return m_value; }
    bool enabled() const { return value(); }

private:
    FontTag m_tag;
    T m_value;
};

template <typename T>
FontTaggedSetting<T>::FontTaggedSetting(const FontTag& tag, T value)
    : m_tag(tag)
    , m_value(value)
{
}

template <typename T>
FontTaggedSetting<T>::FontTaggedSetting(FontTag&& tag, T value)
    : m_tag(WTFMove(tag))
    , m_value(value)
{
}

template <typename T>
bool FontTaggedSetting<T>::operator==(const FontTaggedSetting<T>& other) const
{
    return m_tag == other.m_tag && m_value == other.m_value;
}

template <typename T>
bool FontTaggedSetting<T>::operator<(const FontTaggedSetting<T>& other) const
{
    return (m_tag < other.m_tag) || (m_tag == other.m_tag && m_value < other.m_value);
}

template <typename T>
class FontTaggedSettings {
public:
    void insert(FontTaggedSetting<T>&&);
    bool operator==(const FontTaggedSettings<T>& other) const { return m_list == other.m_list; }
    bool operator!=(const FontTaggedSettings<T>& other) const { return !(*this == other); }

    bool isEmpty() const { return !size(); }
    size_t size() const { return m_list.size(); }
    const FontTaggedSetting<T>& operator[](int index) const { return m_list[index]; }
    const FontTaggedSetting<T>& at(size_t index) const { return m_list.at(index); }

    typename Vector<FontTaggedSetting<T>>::const_iterator begin() const { return m_list.begin(); }
    typename Vector<FontTaggedSetting<T>>::const_iterator end() const { return m_list.end(); }

    unsigned hash() const;

private:
    Vector<FontTaggedSetting<T>> m_list;
};

template <typename T>
void FontTaggedSettings<T>::insert(FontTaggedSetting<T>&& feature)
{
    // This vector will almost always have 0 or 1 items in it. Don't bother with the overhead of a binary search or a hash set.
    size_t i;
    for (i = 0; i < m_list.size(); ++i) {
        if (!(feature < m_list[i]))
            break;
    }
    if (i < m_list.size() && feature.tag() == m_list[i].tag())
        m_list.remove(i);
    m_list.insert(i, WTFMove(feature));
}

typedef FontTaggedSetting<int> FontFeature;
typedef FontTaggedSettings<int> FontFeatureSettings;

template <> unsigned FontFeatureSettings::hash() const;

#if ENABLE(VARIATION_FONTS)

typedef FontTaggedSettings<float> FontVariationSettings;
WTF::TextStream& operator<<(WTF::TextStream&, const FontVariationSettings&);

template <> unsigned FontVariationSettings::hash() const;

#else

struct FontVariationSettings {
    bool isEmpty() const { return true; }
};

#endif

}
