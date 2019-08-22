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
#include "WHLSLNameSpace.h"
#include "WHLSLType.h"
#include <wtf/FastMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class NamedType : public Type {
    WTF_MAKE_FAST_ALLOCATED;

protected:
    ~NamedType() = default;

public:
    NamedType(Kind kind, CodeLocation location, String&& name)
        : Type(kind)
        , m_codeLocation(location)
        , m_name(WTFMove(name))
    {
    }

    NamedType(const NamedType&) = delete;
    NamedType(NamedType&&) = default;

    CodeLocation codeLocation() const { return m_codeLocation; }
    void updateCodeLocation(CodeLocation location) { m_codeLocation = location; }

    String& name() { return m_name; }

    NameSpace nameSpace() const { return m_nameSpace; }
    void setNameSpace(NameSpace nameSpace) { m_nameSpace = nameSpace; }

private:
    friend class Type;
    Type& unifyNodeImpl() { return *this; }
    CodeLocation m_codeLocation;
    String m_name;
    NameSpace m_nameSpace { NameSpace::StandardLibrary };
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(NamedType)

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(NamedType, isNamedType())

#endif
