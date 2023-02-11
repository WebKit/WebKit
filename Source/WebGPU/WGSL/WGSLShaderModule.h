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

namespace WGSL {

class ShaderModule {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ShaderModule(const String& source)
        : ShaderModule(source, { })
    { }
    
    ShaderModule(const String& source, const Configuration& configuration)
        : m_source(source)
        , m_configuration(configuration)
    { }

    const String& source() const { return m_source; }
    const Configuration& configuration() const { return m_configuration; }
    AST::Directive::List& directives() { return m_directives; }
    AST::Function::List& functions() { return m_functions; }
    AST::Structure::List& structures() { return m_structures; }
    AST::Variable::List& variables() { return m_variables; }

private:
    String m_source;
    Configuration m_configuration;
    AST::Directive::List m_directives;
    AST::Function::List m_functions;
    AST::Structure::List m_structures;
    AST::Variable::List m_variables;
};

} // namespace WGSL
