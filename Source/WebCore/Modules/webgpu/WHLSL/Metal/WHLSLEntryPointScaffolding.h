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

#include "WHLSLMangledNames.h"
#include "WHLSLPipelineDescriptor.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class FunctionDefinition;

}

struct EntryPointItems;
class Intrinsics;

namespace Metal {

class TypeNamer;

class EntryPointScaffolding {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EntryPointScaffolding() = default;

    virtual void emitHelperTypes(StringBuilder&, Indentation<4>) = 0;
    virtual void emitSignature(StringBuilder&, MangledFunctionName, Indentation<4>) = 0;
    virtual void emitUnpack(StringBuilder&, Indentation<4>) = 0;
    virtual void emitPack(StringBuilder&, MangledVariableName existingVariableName, MangledVariableName, Indentation<4>) = 0;

    Vector<MangledVariableName>& parameterVariables() { return m_parameterVariables; }

protected:
    EntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<MangledVariableName()>&& generateNextVariableName);

    void emitResourceHelperTypes(StringBuilder&, Indentation<4>, ShaderStage);

    enum class IncludePrecedingComma {
        Yes,
        No
    };
    bool emitResourceSignature(StringBuilder&, IncludePrecedingComma);
    bool emitBuiltInsSignature(StringBuilder&, IncludePrecedingComma);

    void emitMangledInputPath(StringBuilder&, Vector<String>& path);
    void emitMangledOutputPath(StringBuilder&, Vector<String>& path);
    void emitUnpackResourcesAndNamedBuiltIns(StringBuilder&, Indentation<4>);

    AST::FunctionDefinition& m_functionDefinition;
    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    EntryPointItems& m_entryPointItems;
    HashMap<Binding*, size_t>& m_resourceMap;
    Layout& m_layout;
    std::function<MangledVariableName()> m_generateNextVariableName;

    struct LengthInformation {
        MangledStructureElementName elementName;
        MangledVariableName temporaryName;
        unsigned index;
    };
    struct NamedBinding {
        MangledStructureElementName elementName;
        unsigned index;
        Optional<LengthInformation> lengthInformation;
    };
    struct NamedBindGroup {
        MangledTypeName structName;
        MangledVariableName variableName;
        Vector<NamedBinding> namedBindings;
        unsigned argumentBufferIndex;
    };
    Vector<NamedBindGroup> m_namedBindGroups; // Parallel to m_layout

    struct NamedBuiltIn {
        size_t indexInEntryPointItems;
        MangledVariableName variableName;
    };
    Vector<NamedBuiltIn> m_namedBuiltIns;

    Vector<MangledVariableName> m_parameterVariables;
};

class VertexEntryPointScaffolding final : public EntryPointScaffolding {
public:
    VertexEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<MangledVariableName()>&& generateNextVariableName, HashMap<VertexAttribute*, size_t>& matchedVertexAttributes);
    virtual ~VertexEntryPointScaffolding() = default;

private:
    void emitHelperTypes(StringBuilder&, Indentation<4>) override;
    void emitSignature(StringBuilder&, MangledFunctionName, Indentation<4>) override;
    void emitUnpack(StringBuilder&, Indentation<4>) override;
    void emitPack(StringBuilder&, MangledVariableName existingVariableName, MangledVariableName, Indentation<4>) override;

    HashMap<VertexAttribute*, size_t>& m_matchedVertexAttributes;
    MangledTypeName m_stageInStructName;
    MangledTypeName m_returnStructName;
    MangledVariableName m_stageInParameterName;

    struct NamedStageIn {
        size_t indexInEntryPointItems;
        MangledStructureElementName elementName;
        unsigned attributeIndex;
    };
    Vector<NamedStageIn> m_namedStageIns;

    struct NamedOutput {
        MangledStructureElementName elementName;
        MangledOrNativeTypeName internalTypeName;
    };
    Vector<NamedOutput> m_namedOutputs;
};

class FragmentEntryPointScaffolding final : public EntryPointScaffolding {
public:
    FragmentEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<MangledVariableName()>&& generateNextVariableName, HashMap<AttachmentDescriptor*, size_t>& matchedColorAttachments);
    virtual ~FragmentEntryPointScaffolding() = default;

private:
    void emitHelperTypes(StringBuilder&, Indentation<4>) override;
    void emitSignature(StringBuilder&, MangledFunctionName, Indentation<4>) override;
    void emitUnpack(StringBuilder&, Indentation<4>) override;
    void emitPack(StringBuilder&, MangledVariableName existingVariableName, MangledVariableName, Indentation<4>) override;

    MangledTypeName m_stageInStructName;
    MangledTypeName m_returnStructName;
    MangledVariableName m_stageInParameterName;

    struct NamedStageIn {
        size_t indexInEntryPointItems;
        MangledStructureElementName elementName;
        unsigned attributeIndex;
    };
    Vector<NamedStageIn> m_namedStageIns;

    struct NamedOutput {
        MangledStructureElementName elementName;
        MangledOrNativeTypeName internalTypeName;
    };
    Vector<NamedOutput> m_namedOutputs;
};

class ComputeEntryPointScaffolding final : public EntryPointScaffolding {
public:
    ComputeEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<MangledVariableName()>&& generateNextVariableName);
    virtual ~ComputeEntryPointScaffolding() = default;

private:
    void emitHelperTypes(StringBuilder&, Indentation<4>) override;
    void emitSignature(StringBuilder&, MangledFunctionName, Indentation<4>) override;
    void emitUnpack(StringBuilder&, Indentation<4>) override;
    void emitPack(StringBuilder&, MangledVariableName existingVariableName, MangledVariableName, Indentation<4>) override;
};

}

}

}

#endif // ENABLE(WHLSL_COMPILER)
