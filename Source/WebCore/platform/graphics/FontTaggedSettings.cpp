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

#include "config.h"
#include "FontTaggedSettings.h"

#include <wtf/Hasher.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

template <>
unsigned FontFeatureSettings::hash() const
{
    IntegerHasher hasher;
    for (auto& feature : m_list) {
        hasher.add(FourCharacterTagHash::hash(feature.tag()));
        hasher.add(feature.value());
    }
    return hasher.hash();
}

#if ENABLE(VARIATION_FONTS)
template <>
unsigned FontVariationSettings::hash() const
{
    static_assert(sizeof(float) == sizeof(int), "IntegerHasher needs to accept floats too");
    union {
        float f;
        int i;
    } floatToInt;

    IntegerHasher hasher;
    for (auto& variation : m_list) {
        hasher.add(FourCharacterTagHash::hash(variation.tag()));
        floatToInt.f = variation.value();
        hasher.add(floatToInt.i);
    }
    return hasher.hash();
}

TextStream& operator<<(TextStream& ts, const FontVariationSettings& item)
{
    for (unsigned i = 0; i < item.size(); ++i) {
        auto& variation = item.at(i);
        StringBuilder s;
        s.append(variation.tag()[0]);
        s.append(variation.tag()[1]);
        s.append(variation.tag()[2]);
        s.append(variation.tag()[3]);
        ts.dumpProperty(s.toString(), item.at(i).value());
    }
    return ts;
}
#endif

}
