/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#if ENABLE(THREADED_ANIMATIONS)

#include <WebCore/AcceleratedEffectCoordBox.h>
#include <WebCore/Path.h>
#include <wtf/URL.h>
#include <optional>

namespace WebCore {

struct BlendingContext;
struct TransformOperationData;
template<typename> struct AcceleratedEffectInterpolation;

struct AcceleratedEffectReferencePath {
    URL url;
    std::optional<Path> path;
    std::optional<AcceleratedEffectCoordBox> box;

    bool operator==(const AcceleratedEffectReferencePath&) const;
};

// MARK: - Path Evaluation

std::optional<WebCore::Path> tryPath(const AcceleratedEffectReferencePath&, const TransformOperationData&);

// MARK: - Blending

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectReferencePath> {
    auto canBlend(const AcceleratedEffectReferencePath&, const AcceleratedEffectReferencePath&) -> bool;
    auto blend(const AcceleratedEffectReferencePath&, const AcceleratedEffectReferencePath&, const BlendingContext&) -> AcceleratedEffectReferencePath;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
