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

#include "WHLSLMappedBindings.h"
#include "WHLSLPipelineDescriptor.h"
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
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
public:
    EntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<String()>&& generateNextVariableName);
    virtual ~EntryPointScaffolding() = default;

    virtual String helperTypes() = 0;
    virtual String signature(String& functionName) = 0;
    virtual String unpack() = 0;
    virtual String pack(const String& existingVariableName, const String& variableName) = 0;

    MappedBindGroups mappedBindGroups() const;
    Vector<String>& parameterVariables() { return m_parameterVariables; }

protected:
    String resourceHelperTypes();
    Optional<String> resourceSignature();
    Optional<String> builtInsSignature();

    String mangledInputPath(Vector<String>& path);
    String mangledOutputPath(Vector<String>& path);
    String unpackResourcesAndNamedBuiltIns();

    AST::FunctionDefinition& m_functionDefinition;
    Intrinsics& m_intrinsics;
    TypeNamer& m_typeNamer;
    EntryPointItems& m_entryPointItems;
    HashMap<Binding*, size_t>& m_resourceMap;
    Layout& m_layout;
    std::function<String()> m_generateNextVariableName;

    struct NamedBinding {
        String elementName;
        unsigned index;
    };
    struct NamedBindGroup {
        String structName;
        String variableName;
        Vector<NamedBinding> namedBindings;
        unsigned argumentBufferIndex;
    };
    Vector<NamedBindGroup> m_namedBindGroups; // Parallel to m_layout

    struct NamedBuiltIn {
        size_t indexInEntryPointItems;
        String variableName;
    };
    Vector<NamedBuiltIn> m_namedBuiltIns;

    Vector<String> m_parameterVariables;
};

class VertexEntryPointScaffolding : public EntryPointScaffolding {
public:
    VertexEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<String()>&& generateNextVariableName, HashMap<VertexAttribute*, size_t>& matchedVertexAttributes);
    virtual ~VertexEntryPointScaffolding() = default;

    String helperTypes() override;
    String signature(String& functionName) override;
    String unpack() override;
    String pack(const String& existingVariableName, const String& variableName) override;

private:
    HashMap<VertexAttribute*, size_t>& m_matchedVertexAttributes;
    String m_stageInStructName;
    String m_returnStructName;
    String m_stageInParameterName;

    struct NamedStageIn {
        size_t indexInEntryPointItems;
        String elementName;
        unsigned attributeIndex;
    };
    Vector<NamedStageIn> m_namedStageIns;

    struct NamedOutput {
        String elementName;
    };
    Vector<NamedOutput> m_namedOutputs;
};

class FragmentEntryPointScaffolding : public EntryPointScaffolding {
public:
    FragmentEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<String()>&& generateNextVariableName, HashMap<AttachmentDescriptor*, size_t>& matchedColorAttachments);
    virtual ~FragmentEntryPointScaffolding() = default;

    String helperTypes() override;
    String signature(String& functionName) override;
    String unpack() override;
    String pack(const String& existingVariableName, const String& variableName) override;

private:
    String m_stageInStructName;
    String m_returnStructName;
    String m_stageInParameterName;

    struct NamedStageIn {
        size_t indexInEntryPointItems;
        String elementName;
        unsigned attributeIndex;
    };
    Vector<NamedStageIn> m_namedStageIns;

    struct NamedOutput {
        String elementName;
    };
    Vector<NamedOutput> m_namedOutputs;
};

class ComputeEntryPointScaffolding : public EntryPointScaffolding {
public:
    ComputeEntryPointScaffolding(AST::FunctionDefinition&, Intrinsics&, TypeNamer&, EntryPointItems&, HashMap<Binding*, size_t>& resourceMap, Layout&, std::function<String()>&& generateNextVariableName);
    virtual ~ComputeEntryPointScaffolding() = default;

    String helperTypes() override;
    String signature(String& functionName) override;
    String unpack() override;
    String pack(const String& existingVariableName, const String& variableName) override;

private:
};

}

}

}

#endif
