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
#include "WHLSLUnnamedType.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLPointerType.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

unsigned UnnamedType::hash() const
{
    switch (m_kind) {
    case Kind::TypeReference:
        return downcast<TypeReference>(*this).hash();
    case Kind::PointerType:
        return downcast<PointerType>(*this).hash();
    case Kind::ArrayReferenceType:
        return downcast<ArrayReferenceType>(*this).hash();
    case Kind::ArrayType:
        return downcast<ArrayType>(*this).hash();
    }
}

bool UnnamedType::operator==(const UnnamedType& other) const
{
    if (other.m_kind != m_kind)
        return false;

    switch (m_kind) {
    case Kind::TypeReference:
        return downcast<TypeReference>(*this) == downcast<TypeReference>(other);
    case Kind::PointerType:
        return downcast<PointerType>(*this) == downcast<PointerType>(other);
    case Kind::ArrayReferenceType:
        return downcast<ArrayReferenceType>(*this) == downcast<ArrayReferenceType>(other);
    case Kind::ArrayType:
        return downcast<ArrayType>(*this) == downcast<ArrayType>(other);
    }
}

} // namespace AST

} // namespace WHLSL

} // namespace WebCore

#endif
