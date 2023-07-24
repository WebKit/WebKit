/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#include "Constraints.h"

#include "TypeStore.h"

namespace WGSL {

bool satisfies(const Type* type, Constraint constraint)
{
    auto* primitive = std::get_if<Types::Primitive>(type);
    if (!primitive) {
        if (auto* reference = std::get_if<Types::Reference>(type))
            return satisfies(reference->element, constraint);
        return false;
    }

    switch (primitive->kind) {
    case Types::Primitive::AbstractInt:
        return constraint >= Constraints::AbstractInt;
    case Types::Primitive::AbstractFloat:
        return constraint & Constraints::Float;
    case Types::Primitive::I32:
        return constraint & Constraints::I32;
    case Types::Primitive::U32:
        return constraint & Constraints::U32;
    case Types::Primitive::F32:
        return constraint & Constraints::F32;
    // FIXME: Add F16 support
    // https://bugs.webkit.org/show_bug.cgi?id=254668
    case Types::Primitive::Bool:
        return constraint & Constraints::Bool;

    case Types::Primitive::Void:
    case Types::Primitive::Sampler:
    case Types::Primitive::TextureExternal:
        return false;
    }
}

const Type* satisfyOrPromote(const Type* type, Constraint constraint, const TypeStore& types)
{
    auto* primitive = std::get_if<Types::Primitive>(type);
    if (!primitive) {
        if (auto* reference = std::get_if<Types::Reference>(type))
            return satisfyOrPromote(reference->element, constraint, types);
        return nullptr;
    }

    switch (primitive->kind) {
    case Types::Primitive::AbstractInt:
        if (constraint < Constraints::AbstractInt)
            return nullptr;
        if (constraint & Constraints::AbstractInt)
            return type;
        if (constraint & Constraints::I32)
            return types.i32Type();
        if (constraint & Constraints::U32)
            return types.u32Type();
        if (constraint & Constraints::AbstractFloat)
            return types.abstractFloatType();
        if (constraint & Constraints::F32)
            return types.f32Type();
        if (constraint & Constraints::F16) {
            // FIXME: Add F16 support
            // https://bugs.webkit.org/show_bug.cgi?id=254668
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Types::Primitive::AbstractFloat:
        if (constraint < Constraints::AbstractFloat)
            return nullptr;
        if (constraint & Constraints::AbstractFloat)
            return type;
        if (constraint & Constraints::F32)
            return types.f32Type();
        if (constraint & Constraints::F16) {
            // FIXME: Add F16 support
            // https://bugs.webkit.org/show_bug.cgi?id=254668
            RELEASE_ASSERT_NOT_REACHED();
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Types::Primitive::I32:
        if (!(constraint & Constraints::I32))
            return nullptr;
        return type;
    case Types::Primitive::U32:
        if (!(constraint & Constraints::U32))
            return nullptr;
        return type;
    case Types::Primitive::F32:
        if (!(constraint & Constraints::F32))
            return nullptr;
        return type;
    // FIXME: Add F16 support
    // https://bugs.webkit.org/show_bug.cgi?id=254668
    case Types::Primitive::Bool:
        if (!(constraint & Constraints::Bool))
            return nullptr;
        return type;

    case Types::Primitive::Void:
    case Types::Primitive::Sampler:
    case Types::Primitive::TextureExternal:
        return nullptr;
    }
}

} // namespace WGSL
