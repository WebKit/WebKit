/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InternalsMapLike.h"

#include "IDLTypes.h"
#include "JSDOMMapLike.h"
#include <wtf/Vector.h>

namespace WebCore {

InternalsMapLike::InternalsMapLike()
{
    m_values.add("init", 0);
}

void InternalsMapLike::initializeMapLike(DOMMapAdapter& map)
{
    for (auto& keyValue : m_values)
        map.set<IDLDOMString, IDLUnsignedLong>(keyValue.key, keyValue.value);
}

void InternalsMapLike::setFromMapLike(String&& key, unsigned value)
{
    m_values.set(WTFMove(key), value);
}

void InternalsMapLike::clear()
{
    m_values.clear();
}

bool InternalsMapLike::remove(const String& key)
{
    return m_values.remove(key);
}

Vector<String> InternalsMapLike::inspectKeys() const
{
    auto result = copyToVector(m_values.keys());
    std::sort(result.begin(), result.end(), WTF::codePointCompareLessThan);
    return result;
}

Vector<unsigned> InternalsMapLike::inspectValues() const
{
    auto result = copyToVector(m_values.values());
    std::sort(result.begin(), result.end(), std::less<unsigned>());
    return result;
}

} // namespace WebCore
