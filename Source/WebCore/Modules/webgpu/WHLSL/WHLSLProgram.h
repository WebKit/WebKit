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
#include "WHLSLError.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIntrinsics.h"
#include "WHLSLNameContext.h"
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

    Expected<void, Error> append(AST::TypeDefinition&& typeDefinition)
    {
        m_typeDefinitions.append(makeUniqueRef<AST::TypeDefinition>(WTFMove(typeDefinition)));
        return m_nameContext.add(m_typeDefinitions.last());
    }

    Expected<void, Error> append(AST::StructureDefinition&& structureDefinition)
    {
        m_structureDefinitions.append(makeUniqueRef<AST::StructureDefinition>(WTFMove(structureDefinition)));
        return m_nameContext.add(m_structureDefinitions.last());
    }

    Expected<void, Error> append(AST::EnumerationDefinition&& enumerationDefinition)
    {
        m_enumerationDefinitions.append(makeUniqueRef<AST::EnumerationDefinition>(WTFMove(enumerationDefinition)));
        return m_nameContext.add(m_enumerationDefinitions.last());
    }

    Expected<void, Error> append(AST::FunctionDefinition&& functionDefinition)
    {
        m_functionDefinitions.append(makeUniqueRef<AST::FunctionDefinition>(WTFMove(functionDefinition)));
        return m_nameContext.add(m_functionDefinitions.last());
    }

    Expected<void, Error> append(AST::NativeFunctionDeclaration&& nativeFunctionDeclaration)
    {
        m_nativeFunctionDeclarations.append(makeUniqueRef<AST::NativeFunctionDeclaration>(WTFMove(nativeFunctionDeclaration)));
        m_intrinsics.add(m_nativeFunctionDeclarations.last());
        return m_nameContext.add(m_nativeFunctionDeclarations.last());
    }

    Expected<void, Error> append(AST::NativeTypeDeclaration&& nativeTypeDeclaration)
    {
        m_nativeTypeDeclarations.append(makeUniqueRef<AST::NativeTypeDeclaration>(WTFMove(nativeTypeDeclaration)));
        m_intrinsics.add(m_nativeTypeDeclarations.last());
        return m_nameContext.add(m_nativeTypeDeclarations.last());
    }

    NameContext& nameContext() { return m_nameContext; }
    Intrinsics& intrinsics() { return m_intrinsics; }
    const Intrinsics& intrinsics() const { return m_intrinsics; }
    Vector<UniqueRef<AST::TypeDefinition>>& typeDefinitions() { return m_typeDefinitions; }
    Vector<UniqueRef<AST::StructureDefinition>>& structureDefinitions() { return m_structureDefinitions; }
    Vector<UniqueRef<AST::EnumerationDefinition>>& enumerationDefinitions() { return m_enumerationDefinitions; }
    const Vector<UniqueRef<AST::FunctionDefinition>>& functionDefinitions() const { return m_functionDefinitions; }
    Vector<UniqueRef<AST::FunctionDefinition>>& functionDefinitions() { return m_functionDefinitions; }
    const Vector<UniqueRef<AST::NativeFunctionDeclaration>>& nativeFunctionDeclarations() const { return m_nativeFunctionDeclarations; }
    Vector<UniqueRef<AST::NativeFunctionDeclaration>>& nativeFunctionDeclarations() { return m_nativeFunctionDeclarations; }
    Vector<UniqueRef<AST::NativeTypeDeclaration>>& nativeTypeDeclarations() { return m_nativeTypeDeclarations; }

private:
    NameContext m_nameContext;
    Intrinsics m_intrinsics;
    Vector<UniqueRef<AST::TypeDefinition>> m_typeDefinitions;
    Vector<UniqueRef<AST::StructureDefinition>> m_structureDefinitions;
    Vector<UniqueRef<AST::EnumerationDefinition>> m_enumerationDefinitions;
    Vector<UniqueRef<AST::FunctionDefinition>> m_functionDefinitions;
    Vector<UniqueRef<AST::NativeFunctionDeclaration>> m_nativeFunctionDeclarations;
    Vector<UniqueRef<AST::NativeTypeDeclaration>> m_nativeTypeDeclarations;
};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
