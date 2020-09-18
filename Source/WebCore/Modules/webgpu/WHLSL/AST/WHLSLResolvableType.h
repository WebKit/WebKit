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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLType.h"
#include "WHLSLUnnamedType.h"
#include <memory>
#include <wtf/FastMalloc.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ResolvableType : public Type {
    WTF_MAKE_FAST_ALLOCATED;

protected:
    ~ResolvableType() = default;

public:
    ResolvableType(Kind kind)
        : Type(kind)
    { }


    ResolvableType(const ResolvableType&) = delete;
    ResolvableType(ResolvableType&&) = default;

    ResolvableType& operator=(const ResolvableType&) = delete;
    ResolvableType& operator=(ResolvableType&&) = default;

    bool canResolve(const Type&) const;
    unsigned conversionCost(const UnnamedType&) const;

    const UnnamedType* maybeResolvedType() const { return m_resolvedType ? &*m_resolvedType : nullptr; }
    const UnnamedType& resolvedType() const
    {
        ASSERT(m_resolvedType);
        return *m_resolvedType;
    }

    UnnamedType* maybeResolvedType() { return m_resolvedType ? &*m_resolvedType : nullptr; }
    UnnamedType& resolvedType()
    {
        ASSERT(m_resolvedType);
        return *m_resolvedType;
    }

    void resolve(Ref<UnnamedType> type)
    {
        m_resolvedType = WTFMove(type);
    }

private:
    RefPtr<UnnamedType> m_resolvedType;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(ResolvableType)

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(ResolvableType, isResolvableType())

#endif // ENABLE(WHLSL_COMPILER)
