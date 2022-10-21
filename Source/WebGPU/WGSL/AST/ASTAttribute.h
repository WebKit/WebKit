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

class Attribute : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Kind {
        Group,
        Binding,
        Stage,
        Location,
        Builtin,
    };

    using List = UniqueRefVector<Attribute, 2>;

    Attribute(SourceSpan span)
        : ASTNode(span)
    {
    }

    virtual ~Attribute() { };

    virtual Kind kind() const = 0;
    bool isGroup() const { return kind() == Kind::Group; }
    bool isBinding() const { return kind() == Kind::Binding; }
    bool isStage() const { return kind() == Kind::Stage; }
    bool isLocation() const { return kind() == Kind::Location; }
    bool isBuiltin() const { return kind() == Kind::Builtin; }
};

class GroupAttribute final : public Attribute {
    WTF_MAKE_FAST_ALLOCATED;

public:
    GroupAttribute(SourceSpan span, unsigned group)
        : Attribute(span)
        , m_value(group)
    {
    }

    Kind kind() const override { return Kind::Group; }
    unsigned group() const { return m_value; }

private:
    unsigned m_value;
};

class BindingAttribute final : public Attribute {
    WTF_MAKE_FAST_ALLOCATED;

public:
    BindingAttribute(SourceSpan span, unsigned binding)
        : Attribute(span)
        , m_value(binding)
    {
    }

    Kind kind() const override { return Kind::Binding; }
    unsigned binding() const { return m_value; }

private:
    unsigned m_value;
};

class StageAttribute final : public Attribute {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Stage : uint8_t {
        Compute,
        Vertex,
        Fragment
    };

    StageAttribute(SourceSpan span, Stage stage)
        : Attribute(span)
        , m_stage(stage)
    {
    }

    Kind kind() const override { return Kind::Stage; }
    Stage stage() const { return m_stage; }

private:
    Stage m_stage;
};

class BuiltinAttribute final : public Attribute {
    WTF_MAKE_FAST_ALLOCATED;

public:
    BuiltinAttribute(SourceSpan span, StringView name)
        : Attribute(span)
        , m_name(name)
    {
    }

    Kind kind() const override { return Kind::Builtin; }
    const StringView& name() const { return m_name; }

private:
    StringView m_name;
};

class LocationAttribute final : public Attribute {
    WTF_MAKE_FAST_ALLOCATED;

public:
    LocationAttribute(SourceSpan span, unsigned value)
        : Attribute(span)
        , m_value(value)
    {
    }

    Kind kind() const override { return Kind::Location; }
    unsigned location() const { return m_value; }

private:
    unsigned m_value;
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::Attribute& attr) { return attr.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(GroupAttribute, isGroup())
SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(BindingAttribute, isBinding())
SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(StageAttribute, isStage())
SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(LocationAttribute, isLocation())
SPECIALIZE_TYPE_TRAITS_WGSL_ATTRIBUTE(BuiltinAttribute, isBuiltin())
