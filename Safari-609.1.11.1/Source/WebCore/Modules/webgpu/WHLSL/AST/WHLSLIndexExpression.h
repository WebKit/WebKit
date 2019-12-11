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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLCodeLocation.h"
#include "WHLSLPropertyAccessExpression.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class IndexExpression final : public PropertyAccessExpression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IndexExpression(CodeLocation location, UniqueRef<Expression>&& base, UniqueRef<Expression>&& index)
        : PropertyAccessExpression(location, Kind::Index, WTFMove(base))
        , m_index(WTFMove(index))
    {
#if CPU(ADDRESS32)
        UNUSED_PARAM(m_padding);
#endif
    }

    ~IndexExpression() = default;

    IndexExpression(const IndexExpression&) = delete;
    IndexExpression(IndexExpression&&) = default;

    Expression& indexExpression() { return m_index; }
    UniqueRef<Expression>& indexReference() { return m_index; }
    UniqueRef<Expression> takeIndex() { return WTFMove(m_index); }

private:
    UniqueRef<Expression> m_index;
#if CPU(ADDRESS32)
    // This is used to allow for replaceWith into a bigger type.
    char m_padding[4];
#endif
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(IndexExpression)

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(IndexExpression, isIndexExpression())

#endif
