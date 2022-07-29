/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "Texture.h"

namespace WGSL::Semantics::Types {

Texture::Texture(SourceSpan span, Dimension dimension, UniqueRef<Type> componentType)
    : Type(Type::Kind::Texture, span)
    , m_dimension(dimension)
    , m_componentType(WTFMove(componentType))
{
}

Expected<UniqueRef<Type>, Error> Texture::tryCreate(SourceSpan span, Dimension dimension, UniqueRef<Type> componentType)
{
    if (!componentType->isF32() && !componentType->isI32() && !componentType->isU32()) {
        auto err = makeString(
            "texture component type '",
            componentType->string(),
            "' is not f32, i32 or u32"
        );

        return makeUnexpected(Error { WTFMove(err), componentType->span() });
    }

    // return { makeUniqueRef<Texture>(span, dimension, WTFMove(componentType)) };
    return { UniqueRef(*new Texture(span, dimension, WTFMove(componentType))) };
}

bool Texture::operator==(const Type &other) const
{
    if (!other.isTexture())
        return false;

    const auto& textureOther = downcast<Texture>(other);
    return (m_dimension == textureOther.m_dimension)
        && (m_componentType.get() == textureOther.m_componentType.get());
}

} // namespace WGSL::Semantics::Types
