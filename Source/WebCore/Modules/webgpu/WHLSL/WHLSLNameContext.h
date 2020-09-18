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

#include "WHLSLError.h"
#include "WHLSLNameSpace.h"

#include <functional>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class NamedType;
class FunctionDeclaration;
class TypeDefinition;
class StructureDefinition;
class EnumerationDefinition;
class FunctionDefinition;
class NativeFunctionDeclaration;
class NativeTypeDeclaration;
class VariableDeclaration;

}

class NameContext {
public:
    NameContext(NameContext* parent = nullptr);

    void setCurrentNameSpace(AST::NameSpace currentNameSpace)
    {
        ASSERT(!m_parent);
        m_currentNameSpace = currentNameSpace;
    }

    Expected<void, Error> add(AST::TypeDefinition&);
    Expected<void, Error> add(AST::StructureDefinition&);
    Expected<void, Error> add(AST::EnumerationDefinition&);
    Expected<void, Error> add(AST::FunctionDefinition&);
    Expected<void, Error> add(AST::NativeFunctionDeclaration&);
    Expected<void, Error> add(AST::NativeTypeDeclaration&);
    Expected<void, Error> add(AST::VariableDeclaration&);

    Vector<std::reference_wrapper<AST::NamedType>, 1> getTypes(const String&, AST::NameSpace fromNamespace);
    Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1> getFunctions(const String&, AST::NameSpace fromNamespace);
    AST::VariableDeclaration* getVariable(const String&);

private:
    AST::NamedType* searchTypes(String& name) const;
    AST::FunctionDeclaration* searchFunctions(String& name) const;
    Optional<CodeLocation> topLevelExists(String& name) const;
    AST::VariableDeclaration* localExists(String& name) const;

    HashMap<String, Vector<std::reference_wrapper<AST::NamedType>, 1>> m_types[AST::nameSpaceCount];
    HashMap<String, Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>> m_functions[AST::nameSpaceCount];
    HashMap<String, AST::VariableDeclaration*> m_variables;
    NameContext* m_parent;
    AST::NameSpace m_currentNameSpace { AST::NameSpace::StandardLibrary };
};

}

}

#endif // ENABLE(WHLSL_COMPILER)
