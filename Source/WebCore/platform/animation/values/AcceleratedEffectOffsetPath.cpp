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
#include "AcceleratedEffectOffsetPath.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "TransformOperationData.h"

namespace WebCore {

// MARK: - Path Evaluation

std::optional<WebCore::Path> tryPath(const AcceleratedEffectOffsetPath& path, const TransformOperationData& data)
{
    return WTF::switchOn(path.value,
        [](const AcceleratedEffectOffsetPath::None&) -> std::optional<WebCore::Path> {
            return std::nullopt;
        },
        [&](const AcceleratedEffectOffsetPath::RayPath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        },
        [&](const AcceleratedEffectOffsetPath::ReferencePath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        },
        [&](const AcceleratedEffectOffsetPath::BasicShapePath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        },
        [&](const AcceleratedEffectOffsetPath::BoxPath& path) -> std::optional<WebCore::Path> {
            return tryPath(path, data);
        }
    );
}

// MARK: - Blending

bool canBlend(const AcceleratedEffectOffsetPath& from, const AcceleratedEffectOffsetPath& to)
{
    return WTF::visit(WTF::makeVisitor(
        []<typename T>(const T& from, const T& to) {
            return WebCore::canBlendForAcceleratedEffect(from, to);
        },
        [](const AcceleratedEffectOffsetPath::None&, const AcceleratedEffectOffsetPath::None&) {
            return false;
        },
        [](const AcceleratedEffectOffsetPath::ReferencePath&, const AcceleratedEffectOffsetPath::ReferencePath&) {
            return false;
        },
        [](const AcceleratedEffectOffsetPath::BoxPath&, const AcceleratedEffectOffsetPath::BoxPath&) {
            return false;
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), from.value, to.value);
}

AcceleratedEffectOffsetPath blend(const AcceleratedEffectOffsetPath& from, const AcceleratedEffectOffsetPath& to, const BlendingContext& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& from, const T& to) -> AcceleratedEffectOffsetPath {
            return { WebCore::blendForAcceleratedEffect(from, to, context) };
        },
        [](const AcceleratedEffectOffsetPath::None&, const AcceleratedEffectOffsetPath::None&) -> AcceleratedEffectOffsetPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](const AcceleratedEffectOffsetPath::ReferencePath&, const AcceleratedEffectOffsetPath::ReferencePath&) -> AcceleratedEffectOffsetPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](const AcceleratedEffectOffsetPath::BoxPath&, const AcceleratedEffectOffsetPath::BoxPath&) -> AcceleratedEffectOffsetPath {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const auto&, const auto&) -> AcceleratedEffectOffsetPath {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), from.value, to.value);
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
