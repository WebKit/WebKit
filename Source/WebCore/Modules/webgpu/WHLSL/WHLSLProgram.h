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

#include "WHLSLEnumerationDefinition.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace WHLSL {

class Program {
public:
    Program() = default;
    Program(Program&&) = default;

    bool append(AST::TypeDefinition&& typeDefinition)
    {
        m_typeDefinitions.append(makeUniqueRef<AST::TypeDefinition>(WTFMove(typeDefinition)));
        return true;
    }

    bool append(AST::StructureDefinition&& structureDefinition)
    {
        m_structureDefinitions.append(makeUniqueRef<AST::StructureDefinition>(WTFMove(structureDefinition)));
        return true;
    }

    bool append(AST::EnumerationDefinition&& enumerationDefinition)
    {
        m_enumerationDefinitions.append(makeUniqueRef<AST::EnumerationDefinition>(WTFMove(enumerationDefinition)));
        return true;
    }

    bool append(AST::FunctionDefinition&& functionDefinition)
    {
        m_functionDefinitions.append(makeUniqueRef<AST::FunctionDefinition>(WTFMove(functionDefinition)));
        return true;
    }

    bool append(AST::NativeFunctionDeclaration&& nativeFunctionDeclaration)
    {
        m_nativeFunctionDeclarations.append(makeUniqueRef<AST::NativeFunctionDeclaration>(WTFMove(nativeFunctionDeclaration)));
        return true;
    }

    bool append(AST::NativeTypeDeclaration&& nativeTypeDeclaration)
    {
        m_nativeTypeDeclarations.append(makeUniqueRef<AST::NativeTypeDeclaration>(WTFMove(nativeTypeDeclaration)));
        return true;
    }

    Vector<UniqueRef<AST::TypeDefinition>>& typeDefinitions() { return m_typeDefinitions; }
    Vector<UniqueRef<AST::StructureDefinition>>& structureDefinitions() { return m_structureDefinitions; }
    Vector<UniqueRef<AST::EnumerationDefinition>>& enumerationDefinitions() { return m_enumerationDefinitions; }
    const Vector<UniqueRef<AST::FunctionDefinition>>& functionDefinitions() const { return m_functionDefinitions; }
    Vector<UniqueRef<AST::FunctionDefinition>>& functionDefinitions() { return m_functionDefinitions; }
    const Vector<UniqueRef<AST::NativeFunctionDeclaration>>& nativeFunctionDeclarations() const { return m_nativeFunctionDeclarations; }
    Vector<UniqueRef<AST::NativeFunctionDeclaration>>& nativeFunctionDeclarations() { return m_nativeFunctionDeclarations; }
    Vector<UniqueRef<AST::NativeTypeDeclaration>>& nativeTypeDeclarations() { return m_nativeTypeDeclarations; }

private:
    Vector<UniqueRef<AST::TypeDefinition>> m_typeDefinitions;
    Vector<UniqueRef<AST::StructureDefinition>> m_structureDefinitions;
    Vector<UniqueRef<AST::EnumerationDefinition>> m_enumerationDefinitions;
    Vector<UniqueRef<AST::FunctionDefinition>> m_functionDefinitions;
    Vector<UniqueRef<AST::NativeFunctionDeclaration>> m_nativeFunctionDeclarations;
    Vector<UniqueRef<AST::NativeTypeDeclaration>> m_nativeTypeDeclarations;
};

}

}

#endif
