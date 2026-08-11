/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "AcceleratedEffectClipPath.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "TransformOperationData.h"

namespace WebCore {

// MARK: - Path Evaluation

std::optional<WebCore::Path> tryPath(const AcceleratedEffectClipPath& path, const TransformOperationData& data)
{
    return WTF::switchOn(path.value,
        [](const AcceleratedEffectClipPath::None&) -> std::optional<WebCore::Path> {
            return std::nullopt;
        },
        [&](const AcceleratedEffectClipPath::ReferencePath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        },
        [&](const AcceleratedEffectClipPath::BasicShapePath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        },
        [&](const AcceleratedEffectClipPath::BoxPath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        }
    );
}

// MARK: - Blending

bool canBlend(const AcceleratedEffectClipPath& from, const AcceleratedEffectClipPath& to)
{
    return WTF::visit(WTF::makeVisitor(
        []<typename T>(const T& from, const T& to)
        {
            return WebCore::canBlendForAcceleratedEffect(from, to);
        },
        [](const AcceleratedEffectClipPath::None&, const AcceleratedEffectClipPath::None&) {
            return false;
        },
        [](const AcceleratedEffectClipPath::ReferencePath&, const AcceleratedEffectClipPath::ReferencePath&) {
            return false;
        },
        [](const AcceleratedEffectClipPath::BoxPath&, const AcceleratedEffectClipPath::BoxPath&) {
            return false;
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), from.value, to.value);
}

AcceleratedEffectClipPath blend(const AcceleratedEffectClipPath& from, const AcceleratedEffectClipPath& to, const BlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& from, const T& to) -> AcceleratedEffectClipPath {
            return { WebCore::blendForAcceleratedEffect(from, to, context) };
        },
        [](const AcceleratedEffectClipPath::None&, const AcceleratedEffectClipPath::None&) -> AcceleratedEffectClipPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](const AcceleratedEffectClipPath::ReferencePath&, const AcceleratedEffectClipPath::ReferencePath&) -> AcceleratedEffectClipPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](const AcceleratedEffectClipPath::BoxPath&, const AcceleratedEffectClipPath::BoxPath&) -> AcceleratedEffectClipPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const auto&, const auto&) -> AcceleratedEffectClipPath {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), from.value, to.value);
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
