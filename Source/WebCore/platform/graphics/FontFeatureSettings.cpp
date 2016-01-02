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

#include "config.h"
#include "FontFeatureSettings.h"

#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

FontFeature::FontFeature(const FontFeatureTag& tag, int value)
    : m_tag(tag)
    , m_value(value)
{
}

FontFeature::FontFeature(FontFeatureTag&& tag, int value)
    : m_tag(WTFMove(tag))
    , m_value(value)
{
}

bool FontFeature::operator==(const FontFeature& other) const
{
    return m_tag == other.m_tag && m_value == other.m_value;
}

bool FontFeature::operator<(const FontFeature& other) const
{
    return (m_tag < other.m_tag) || (m_tag == other.m_tag && m_value < other.m_value);
}

void FontFeatureSettings::insert(FontFeature&& feature)
{
    // This vector will almost always have 0 or 1 items in it. Don't bother with the overhead of a binary search or a hash set.
    size_t i;
    for (i = 0; i < m_list.size(); ++i) {
        if (feature < m_list[i])
            break;
    }
    m_list.insert(i, WTFMove(feature));
}

unsigned FontFeatureSettings::hash() const
{
    IntegerHasher hasher;
    for (auto& feature : m_list) {
        hasher.add(FontFeatureTagHash::hash(feature.tag()));
        hasher.add(feature.value());
    }
    return hasher.hash();
}

}
