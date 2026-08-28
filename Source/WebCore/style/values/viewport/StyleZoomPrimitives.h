/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {

class RenderElement;

namespace Style {

class ComputedStyle;

struct ZoomFactor {
    float value;

    constexpr explicit ZoomFactor(float v) : value { v } { }

    // Special zoom factor to use when zoom has already been applied to a value.
    static constexpr ZoomFactor none() { return ZoomFactor { 1 }; }

    constexpr bool operator==(const ZoomFactor&) const = default;
};

// Map from values with zoom applied to values which are zoom-independent.
template<typename T>
T unapplyingZoom(T, const ComputedStyle&);

template<typename T>
T unapplyingZoom(T, const RenderElement&);

// Map from values which are zoom-independent to values with zoom applied.
template<typename T>
T applyingZoom(T, const ComputedStyle&);

template<typename T>
T applyingZoom(T, const RenderElement&);

} // namespace Style
} // namespace WebCore
