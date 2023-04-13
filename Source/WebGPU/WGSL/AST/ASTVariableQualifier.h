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

namespace WGSL::AST {

enum class StorageClass : uint8_t {
    Function,
    Private,
    Workgroup,
    Uniform,
    Storage
};

enum class AccessMode : uint8_t {
    Read,
    Write,
    ReadWrite
};

// FIXME: Perhaps this class is not needed if we have spanned identifier?
class VariableQualifier final : public Node {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Ptr = std::unique_ptr<VariableQualifier>;

    VariableQualifier(SourceSpan span, StorageClass storageClass, AccessMode accessMode)
        : Node(span)
        , m_storageClass(storageClass)
        , m_accessMode(accessMode)
    { }

    NodeKind kind() const override;
    StorageClass storageClass() const { return m_storageClass; }
    AccessMode accessMode() const { return m_accessMode; }

private:
    StorageClass m_storageClass;
    AccessMode m_accessMode;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(VariableQualifier)
