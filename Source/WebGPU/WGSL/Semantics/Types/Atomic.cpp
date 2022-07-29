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
#include "Atomic.h"

namespace WGSL::Semantics::Types {

static std::optional<Error> checkValidInnerType(const Type& type, SourceSpan span)
{
    if (!type.isIntegerScalar()) {
        auto err = makeString(
            "'",
            type.string(),
            "' is not a integer scalar, required by 'atomic'"
        );

        return Error { WTFMove(err), span };
    }

    return { };
}

Atomic::Atomic(SourceSpan span, UniqueRef<Type> innerType)
    : Type(Type::Kind::Atomic, span)
    , m_innerType(WTFMove(innerType))
{
}

Expected<UniqueRef<Atomic>, Error> Atomic::tryCreate(SourceSpan span, UniqueRef<Type> innerType)
{
    if (auto err = checkValidInnerType(innerType, span))
        return makeUnexpected(*err);

    // return makeUniqueRef<Atomic>(WTFMove(innerType));
    return { UniqueRef(*new Atomic(span, WTFMove(innerType))) };
}

bool Atomic::operator==(const Type& other) const
{
    if (!is<Atomic>(other))
        return false;

    const Atomic& atomicOther = downcast<Atomic>(other);

    return m_innerType.get() == atomicOther.m_innerType.get();
}

String Atomic::string() const
{
    return makeString("atomic<", m_innerType->string(), ">");
}

} // namespace WGSL::Semantics::Types
