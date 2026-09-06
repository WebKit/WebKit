/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

namespace WebCore {
namespace Style {

class ComputedStyleProperties;

// Stores colors from a computed style that a CurrentColor will need to resolve.
class ResolvedColors {
    WTF_MAKE_TZONE_ALLOCATED(ResolvedColors);

public:
    explicit ResolvedColors(WebCore::Color currentColor, WebCore::Color accentColor);

    static ResolvedColors fromStyle(const ComputedStyleProperties&);

    // This grabs the visited link color from the style, instead of the 'normal' color.
    static ResolvedColors fromVisitedLinkStyle(const ComputedStyleProperties&);

    WebCore::Color currentColor() const { return m_currentColor; }
    WebCore::Color accentColor() const { return m_accentColor; }

private:
    WebCore::Color m_currentColor;
    WebCore::Color m_accentColor;
};

} // namespace Style
} // namespace WebCore
