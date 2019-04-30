/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(DARK_MODE_CSS)

#include "RenderStyleConstants.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class StyleColorScheme {
public:
    StyleColorScheme() { }

    StyleColorScheme(OptionSet<ColorScheme> colorScheme, bool allowsTransformations)
        : m_colorScheme(colorScheme)
        , m_allowsTransformations(allowsTransformations)
    { }

    bool operator==(const StyleColorScheme& other) const
    {
        return m_colorScheme == other.m_colorScheme && m_allowsTransformations == other.m_allowsTransformations;
    }

    bool operator!=(const StyleColorScheme& other) const { return !(*this == other); }

    bool isAuto() const { return m_colorScheme.isEmpty() && m_allowsTransformations; }
    bool isOnly() const { return m_colorScheme.isEmpty() && !m_allowsTransformations; }

    OptionSet<ColorScheme> colorScheme() const { return m_colorScheme; }

    void add(ColorScheme colorScheme) { m_colorScheme.add(colorScheme); }
    bool contains(ColorScheme colorScheme) const { return m_colorScheme.contains(colorScheme); }

    void setAllowsTransformations(bool allow) { m_allowsTransformations = allow; }
    bool allowsTransformations() const { return m_allowsTransformations; }

private:
    OptionSet<ColorScheme> m_colorScheme;
    bool m_allowsTransformations { true };
};

} // namespace WebCore

#endif // ENABLE(DARK_MODE_CSS)
