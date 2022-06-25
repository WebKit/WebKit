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
#include <wtf/TypeCasts.h>

namespace WGSL::AST {

class GlobalDecl : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Kind {
        GlobalVariable,
        Struct,
        Function,
    };

    GlobalDecl(SourceSpan span)
        : ASTNode(span)
    {
    }

    virtual ~GlobalDecl() {}

    virtual Kind kind() const = 0;
    bool isGlobalVariable() const { return kind() == Kind::GlobalVariable; }
    bool isStruct() const { return kind() == Kind::Struct; }
    bool isFunction() const { return kind() == Kind::Function; }
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_GLOBAL_DECL(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::GlobalDecl& decl) { return decl.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
