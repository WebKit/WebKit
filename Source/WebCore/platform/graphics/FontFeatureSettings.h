/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef FontFeatureSettings_h
#define FontFeatureSettings_h

#include <array>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

typedef std::array<char, 4> FontFeatureTag;

inline FontFeatureTag fontFeatureTag(const char arr[4]) { return {{ arr[0], arr[1], arr[2], arr[3] }}; }

struct FontFeatureTagHash {
    static unsigned hash(const FontFeatureTag& characters) { return (characters[0] << 24) | (characters[1] << 16) | (characters[2] << 8) | characters[3]; }
    static bool equal(const FontFeatureTag& a, const FontFeatureTag& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

#define _0xFF static_cast<char>(0xFF)
struct FontFeatureTagHashTraits : WTF::GenericHashTraits<FontFeatureTag> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(FontFeatureTag& slot) { new (NotNull, std::addressof(slot)) FontFeatureTag({{ _0xFF, _0xFF, _0xFF, _0xFF }}); }
    static bool isDeletedValue(const FontFeatureTag& value) { return value == FontFeatureTag({{ _0xFF, _0xFF, _0xFF, _0xFF }}); }
};
#undef _0xFF

class FontFeature {
public:
    FontFeature() = delete;
    FontFeature(const FontFeatureTag&, int value);
    FontFeature(FontFeatureTag&&, int value);

    bool operator==(const FontFeature& other) const;
    bool operator!=(const FontFeature& other) const { return !(*this == other); }
    bool operator<(const FontFeature& other) const;

    const FontFeatureTag& tag() const { return m_tag; }
    int value() const { return m_value; }
    bool enabled() const { return value(); }

private:
    FontFeatureTag m_tag;
    int m_value;
};

class FontFeatureSettings {
public:
    void insert(FontFeature&&);
    bool operator==(const FontFeatureSettings& other) const { return m_list == other.m_list; }
    bool operator!=(const FontFeatureSettings& other) const { return !(*this == other); }

    size_t size() const { return m_list.size(); }
    const FontFeature& operator[](int index) const { return m_list[index]; }
    const FontFeature& at(size_t index) const { return m_list.at(index); }

    Vector<FontFeature>::const_iterator begin() const { return m_list.begin(); }
    Vector<FontFeature>::const_iterator end() const { return m_list.end(); }

    unsigned hash() const;

private:
    Vector<FontFeature> m_list;
};

}

#endif // FontFeatureSettings_h
