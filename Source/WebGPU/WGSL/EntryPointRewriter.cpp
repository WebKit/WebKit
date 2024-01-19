/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "EntryPointRewriter.h"

#include "AST.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(ShaderModule&, const AST::Function&, ShaderStage, Reflection::EntryPointInformation&);

    void rewrite();

private:
    struct MemberOrParameter {
        AST::Identifier name;
        AST::Expression& type;
        AST::Attribute::List attributes;
    };

    struct BuiltinMemberOrParameter : MemberOrParameter {
        Builtin builtin;
    };

    enum class IsBuiltin {
        No = 0,
        Yes = 1,
    };

    void collectParameters();
    void checkReturnType();
    void constructInputStruct();
    void materialize(Vector<String>& path, MemberOrParameter&, IsBuiltin, const String* builtinName = nullptr);
    void visit(Vector<String>& path, MemberOrParameter&&);
    void appendBuiltins();

    ShaderStage m_stage;
    ShaderModule& m_shaderModule;
    const AST::Function& m_function;

    Vector<BuiltinMemberOrParameter> m_builtins;
    Vector<MemberOrParameter> m_parameters;
    AST::Statement::List m_materializations;
    const Type* m_structType;
    String m_structTypeName;
    String m_structParameterName;
    Reflection::EntryPointInformation& m_information;
    unsigned m_builtinID { 0 };
};

EntryPointRewriter::EntryPointRewriter(ShaderModule& shaderModule, const AST::Function& function, ShaderStage stage, Reflection::EntryPointInformation& information)
    : m_stage(stage)
    , m_shaderModule(shaderModule)
    , m_function(function)
    , m_information(information)
{
    switch (m_stage) {
    case ShaderStage::Compute: {
        for (auto& attribute : function.attributes()) {
            if (!is<AST::WorkgroupSizeAttribute>(attribute))
                continue;
            auto& workgroupSize = downcast<AST::WorkgroupSizeAttribute>(attribute);
            m_information.typedEntryPoint = Reflection::Compute { &workgroupSize.x(), workgroupSize.maybeY(), workgroupSize.maybeZ() };
            break;
        }
        break;
    }
    case ShaderStage::Vertex:
        m_information.typedEntryPoint = Reflection::Vertex { false };
        break;
    case ShaderStage::Fragment:
        m_information.typedEntryPoint = Reflection::Fragment { };
        break;
    }
}

void EntryPointRewriter::rewrite()
{
    m_structTypeName = makeString("__", m_function.name(), "_inT");
    m_structParameterName = makeString("__", m_function.name(), "_in");

    collectParameters();
    checkReturnType();
    appendBuiltins();

    if (!m_parameters.isEmpty()) {
        constructInputStruct();

        // add parameter to builtins: ${structName} : ${structType}
        auto& type = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(m_structTypeName));
        type.m_inferredType = m_structType;
        auto& parameter = m_shaderModule.astBuilder().construct<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(m_structParameterName),
            type,
            AST::Attribute::List { },
            AST::ParameterRole::StageIn
        );
        m_shaderModule.append(m_function.parameters(), parameter);
    }

    m_shaderModule.insertVector(m_function.body().statements(), 0, m_materializations);
}

void EntryPointRewriter::collectParameters()
{
    for (auto& parameter : m_function.parameters()) {
        Vector<String> path;
        visit(path, MemberOrParameter { parameter.name(), const_cast<AST::Expression&>(parameter.typeName()), parameter.attributes() });
    }
    m_shaderModule.clear(m_function.parameters());
}

void EntryPointRewriter::checkReturnType()
{
    if (m_stage == ShaderStage::Compute)
        return;

    auto* maybeReturnType = m_function.maybeReturnType();
    if (!maybeReturnType)
        return;
    if (!is<AST::IdentifierExpression>(*maybeReturnType))
        return;

    auto& namedTypeName = downcast<AST::IdentifierExpression>(*maybeReturnType);
    if (auto* structType = std::get_if<Types::Struct>(namedTypeName.inferredType())) {
        const auto& duplicateStruct = [&] (AST::StructureRole role, const char* suffix) {
            ASSERT(structType->structure.role() == AST::StructureRole::UserDefined);
            String returnStructName = makeString("__", structType->structure.name(), "_", suffix);
            auto& returnStruct = m_shaderModule.astBuilder().construct<AST::Structure>(
                SourceSpan::empty(),
                AST::Identifier::make(returnStructName),
                AST::StructureMember::List(structType->structure.members()),
                AST::Attribute::List { },
                role
            );
            m_shaderModule.append(m_shaderModule.declarations(), returnStruct);
            auto& returnType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
                SourceSpan::empty(),
                AST::Identifier::make(returnStructName)
            );
            returnType.m_inferredType = m_shaderModule.types().structType(returnStruct);
            m_shaderModule.replace(namedTypeName, returnType);
        };

        if (m_stage == ShaderStage::Fragment) {
            duplicateStruct(AST::StructureRole::FragmentOutput, "FragmentOutput");
            return;
        }

        duplicateStruct(AST::StructureRole::VertexOutput, "VertexOutput");
        return;
    }

    if (m_stage != ShaderStage::Fragment || !m_function.returnTypeBuiltin().has_value())
        return;

    String returnStructName = makeString("__", m_function.name(), "_FragmentOutput");
    auto& fieldType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(namedTypeName.identifier())
    );
    fieldType.m_inferredType = namedTypeName.inferredType();
    auto& member = m_shaderModule.astBuilder().construct<AST::StructureMember>(
        SourceSpan::empty(),
        AST::Identifier::make("__value"_s),
        fieldType,
        AST::Attribute::List(m_function.returnAttributes())
    );
    auto& returnStruct = m_shaderModule.astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(returnStructName),
        AST::StructureMember::List({ member }),
        AST::Attribute::List { },
        AST::StructureRole::FragmentOutputWrapper
    );
    m_shaderModule.append(m_shaderModule.declarations(), returnStruct);
    auto& returnType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(returnStructName)
    );
    returnType.m_inferredType = m_shaderModule.types().structType(returnStruct);
    m_shaderModule.replace(namedTypeName, returnType);
}

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructureMember::List structMembers;
    for (auto& parameter : m_parameters) {
        structMembers.append(m_shaderModule.astBuilder().construct<AST::StructureMember>(
            SourceSpan::empty(),
            WTFMove(parameter.name),
            parameter.type,
            WTFMove(parameter.attributes)
        ));
    }

    AST::StructureRole role;
    switch (m_stage) {
    case ShaderStage::Compute:
        role = AST::StructureRole::ComputeInput;
        break;
    case ShaderStage::Vertex:
        role = AST::StructureRole::VertexInput;
        break;
    case ShaderStage::Fragment:
        role = AST::StructureRole::FragmentInput;
        break;
    }

    auto& structure = m_shaderModule.astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(m_structTypeName),
        WTFMove(structMembers),
        AST::Attribute::List { },
        role
    );
    m_shaderModule.append(m_shaderModule.declarations(), structure);
    m_structType = m_shaderModule.types().structType(structure);
}

void EntryPointRewriter::materialize(Vector<String>& path, MemberOrParameter& data, IsBuiltin isBuiltin, const String* builtinName)
{
    AST::Expression::Ptr rhs;
    if (isBuiltin == IsBuiltin::Yes)
        rhs = &m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(*builtinName));
    else {
        rhs = &m_shaderModule.astBuilder().construct<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(m_structParameterName)),
            AST::Identifier::make(data.name)
        );
    }

    if (!path.size()) {
        m_materializations.append(m_shaderModule.astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.name),
                nullptr, // TODO: do we need a VariableQualifier?
                &data.type,
                rhs,
                AST::Attribute::List { }
            )
        ));
        return;
    }

    path.append(data.name);
    unsigned i = 0;
    AST::Expression::Ref lhs = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(path[i++]));
    while (i < path.size()) {
        lhs = m_shaderModule.astBuilder().construct<AST::FieldAccessExpression>(
            SourceSpan::empty(),
            WTFMove(lhs),
            AST::Identifier::make(path[i++])
        );
    }
    path.removeLast();
    m_materializations.append(m_shaderModule.astBuilder().construct<AST::AssignmentStatement>(
        SourceSpan::empty(),
        WTFMove(lhs),
        *rhs
    ));
}

void EntryPointRewriter::visit(Vector<String>& path, MemberOrParameter&& data)
{
    if (auto* structType = std::get_if<Types::Struct>(data.type.inferredType())) {
        m_materializations.append(m_shaderModule.astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            m_shaderModule.astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(data.name),
                nullptr,
                &data.type,
                nullptr,
                AST::Attribute::List { }
            )
        ));
        path.append(data.name);
        for (auto& member : structType->structure.members())
            visit(path, MemberOrParameter { member.name(), member.type(), member.attributes() });
        path.removeLast();
        return;
    }

    std::optional<Builtin> builtin;
    for (auto& attribute : data.attributes) {
        if (is<AST::BuiltinAttribute>(attribute)) {
            builtin = downcast<AST::BuiltinAttribute>(attribute).builtin();
            break;
        }
    }

    if (builtin.has_value()) {
        if (!path.isEmpty()) {
            // builtin was hoisted from a struct into a parameter, we need to reconstruct the struct
            // ${path}.${data.name} = __builtin${builtinID}
            // Note that we don't use ${data.name} on the right-hand side because it's the name of a
            // struct field, and it might not be unique.
            auto builtinName = makeString("__builtin", String::number(m_builtinID++));
            materialize(path, data, IsBuiltin::Yes, &builtinName);
            m_builtins.append({
                {
                    AST::Identifier::make(builtinName),
                    data.type,
                    data.attributes
                },
                *builtin
            });
            return;
        }

        // if path is empty, then it was already a parameter and there's nothing to do
        m_builtins.append({ data, *builtin });
        return;
    }

    // parameter was moved into a struct, so we need to reload it
    // ${path}.${data.name} = ${struct}.${data.name}
    materialize(path, data, IsBuiltin::No);
    m_parameters.append(WTFMove(data));
}

void EntryPointRewriter::appendBuiltins()
{
    for (auto& data : m_builtins) {
        auto& parameter = m_shaderModule.astBuilder().construct<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(data.name),
            data.type,
            WTFMove(data.attributes),
            AST::ParameterRole::UserDefined
        );
        parameter.m_builtin = data.builtin;
        m_shaderModule.append(m_function.parameters(), parameter);
    }
}

void rewriteEntryPoints(CallGraph& callGraph)
{
    for (auto& entryPoint : callGraph.entrypoints()) {
        EntryPointRewriter rewriter(callGraph.ast(), entryPoint.function, entryPoint.stage, entryPoint.information);
        rewriter.rewrite();
    }
}

} // namespace WGSL
