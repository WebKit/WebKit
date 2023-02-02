/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ASTDirective.h"
#include "ASTFunction.h"
#include "ASTStructure.h"
#include "ASTVariable.h"
#include "WGSL.h"

#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class ShaderModule final : Node {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShaderModule(SourceSpan span, const String& source, const Configuration& configuration, Directive::List&& directives, Node::List&& decls)
        : Node(span)
        , m_source(source)
        , m_configuration(configuration)
        , m_directives(WTFMove(directives))
    {
        for (size_t i = decls.size(); i > 0; --i) {
            UniqueRef<Node> decl = decls.takeLast();
            // We want to go from UniqueRef<BaseClass> to UniqueRef<DerivedClass>, but that is not supported by downcast.
            // So instead we do UniqueRef -> unique_ptr -> raw_pointer -(downcast)-> raw_pointer -> unique_ptr -> UniqueRef...
            if (is<Function>(decl)) {
                Node* rawPtr = decl.moveToUniquePtr().release();
                Function* func = downcast<Function>(rawPtr);
                m_functions.insert(0, makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<Function>(func)));
            } else if (is<Structure>(decl)) {
                Node* rawPtr = decl.moveToUniquePtr().release();
                Structure* downcastedRawPtr = downcast<Structure>(rawPtr);
                m_structures.insert(0, makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<Structure>(downcastedRawPtr)));
            } else if (is<Variable>(decl.get())) {
                Node* rawPtr = decl.moveToUniquePtr().release();
                Variable* downcastedRawPtr = downcast<Variable>(rawPtr);
                m_variables.insert(0, makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<Variable>(downcastedRawPtr)));
            } else {
                ASSERT_NOT_REACHED("Invalid ShaderModule node");
            }
        }
    }

    NodeKind kind() const override;

    const String& source() const { return m_source; }
    const Configuration& configuration() const { return m_configuration; }
    Directive::List& directives() { return m_directives; }
    Function::List& functions() { return m_functions; }
    Structure::List& structures() { return m_structures; }
    Variable::List& variables() { return m_variables; }

private:
    String m_source;
    Configuration m_configuration;
    Directive::List m_directives;
    Function::List m_functions;
    Structure::List m_structures;
    Variable::List m_variables;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(ShaderModule)
