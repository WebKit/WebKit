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
#include "WHLSLQualifier.h"
#include "WHLSLSemantic.h"
#include "WHLSLType.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class StructureElement final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StructureElement(CodeLocation location, Qualifiers&& qualifiers, Ref<UnnamedType> type, String&& name, std::unique_ptr<Semantic>&& semantic)
        : m_codeLocation(location)
        , m_qualifiers(WTFMove(qualifiers))
        , m_type(WTFMove(type))
        , m_name(WTFMove(name))
        , m_semantic(WTFMove(semantic))
    {
    }

    ~StructureElement() = default;

    StructureElement(const StructureElement&) = delete;
    StructureElement(StructureElement&&) = default;

    const CodeLocation& codeLocation() const { return m_codeLocation; }
    UnnamedType& type() { return m_type; }
    const String& name() { return m_name; }
    Semantic* semantic() { return m_semantic.get(); }

private:
    CodeLocation m_codeLocation;
    Qualifiers m_qualifiers;
    Ref<UnnamedType> m_type;
    String m_name;
    std::unique_ptr<Semantic> m_semantic;
};

using StructureElements = Vector<StructureElement>;

} // namespace AST

}

}

#endif
