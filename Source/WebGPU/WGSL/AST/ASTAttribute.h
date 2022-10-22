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

#pragma once

#include "ASTNode.h"

#include <wtf/UniqueRef.h>
#include <wtf/UniqueRefVector.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WGSL::AST {

struct BindingAttribute {
    unsigned binding;
};

struct BuiltinAttribute {
    StringView name;
};

struct GroupAttribute {
    unsigned group;
};

struct LocationAttribute {
    unsigned location;
};

enum struct StageAttribute : uint8_t {
    Compute,
    Fragment,
    Vertex,
};

using AttributeBase = std::variant<BindingAttribute, BuiltinAttribute, GroupAttribute, LocationAttribute, StageAttribute>;

class Attribute : public ASTNode, public AttributeBase {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<Attribute, 2>;

    template<class... Args>
    Attribute(SourceSpan span, Args&&... args)
        : ASTNode(span)
        , AttributeBase(std::forward<Args>(args)...)
    {
    }

    virtual ~Attribute() = default;
};

} // namespace WGSL::AST

