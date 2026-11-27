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
#include "TypeStore.h"
#include "Types.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"
#include <wtf/Borrow.h>
#include <wtf/text/MakeString.h>

namespace WGSL {

class EntryPointRewriter {
public:
    EntryPointRewriter(ShaderModule&, const AST::Function&, ShaderStage);

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
    unsigned m_builtinID { 0 };
    bool m_usesWorkgroupIndex { false };
    bool m_usesGlobalInvocationIndex { false };
};

EntryPointRewriter::EntryPointRewriter(ShaderModule& shaderModule, const AST::Function& function, ShaderStage stage)
    : m_stage(stage)
    , m_shaderModule(shaderModule)
    , m_function(function)
{
}

void EntryPointRewriter::rewrite()
{
    m_structTypeName = makeString("__"_s, m_function.name(), "_inT"_s);
    m_structParameterName = makeString("__"_s, m_function.name(), "_in"_s);

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
    auto* namedTypeName = dynamicDowncast<AST::IdentifierExpression>(m_function.maybeReturnType());

    if (namedTypeName) {
        if (auto* structType = std::get_if<Types::Struct>(namedTypeName->inferredType())) {
            const auto& duplicateStruct = [&] (AST::StructureRole role, ASCIILiteral suffix) {
                ASSERT(structType->structure.role() == AST::StructureRole::UserDefined);
                String returnStructName = makeString("__"_s, structType->structure.name(), '_', suffix);
                auto& returnStruct = m_shaderModule.astBuilder().construct<AST::Structure>(
                    SourceSpan::empty(),
                    AST::Identifier::make(returnStructName),
                    AST::StructureMember::List(structType->structure.members()),
                    role
                );
                m_shaderModule.append(m_shaderModule.declarations(), returnStruct);
                auto& returnType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
                    SourceSpan::empty(),
                    AST::Identifier::make(returnStructName)
                );
                returnType.m_inferredType = m_shaderModule.types().structType(returnStruct);
                returnType.m_evaluation = namedTypeName->evaluation();
                m_shaderModule.replace(*namedTypeName, returnType);
            };

            if (m_stage == ShaderStage::Fragment) {
                duplicateStruct(AST::StructureRole::FragmentOutput, "FragmentOutput"_s);
                return;
            }

            duplicateStruct(AST::StructureRole::VertexOutput, "VertexOutput"_s);
            return;
        }
    }

    if (!m_function.maybeReturnType() || (m_stage != ShaderStage::Fragment && m_stage != ShaderStage::Vertex) || m_function.returnAttributes().isEmpty())
        return;

    auto stageName = m_stage == ShaderStage::Fragment ? "Fragment"_s : "Vertex"_s;
    String returnStructName = makeString("__"_s, m_function.name(), '_', stageName, "Output"_s);
    auto& fieldType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make("__type"_s)
    );
    fieldType.m_inferredType = m_function.maybeReturnType()->inferredType();
    auto& member = m_shaderModule.astBuilder().construct<AST::StructureMember>(
        SourceSpan::empty(),
        AST::Identifier::make("__value"_s),
        fieldType,
        AST::Attribute::List(m_function.returnAttributes())
    );
    auto role = m_stage == ShaderStage::Fragment ? AST::StructureRole::FragmentOutputWrapper : AST::StructureRole::VertexOutputWrapper;
    auto& returnStruct = m_shaderModule.astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(returnStructName),
        AST::StructureMember::List({ member }),
        role
    );
    m_shaderModule.append(m_shaderModule.declarations(), returnStruct);
    auto& returnType = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(returnStructName)
    );
    returnType.m_inferredType = m_shaderModule.types().structType(returnStruct);
    returnType.m_evaluation = m_function.maybeReturnType()->evaluation();

    if (namedTypeName)
        m_shaderModule.replace(*namedTypeName, returnType);
    else if (auto* elaboratedExpression = dynamicDowncast<AST::ElaboratedTypeExpression>(m_function.maybeReturnType()))
        m_shaderModule.replace(*elaboratedExpression, returnType);
    else if (auto* arrayType = dynamicDowncast<AST::ArrayTypeExpression>(m_function.maybeReturnType()))
        m_shaderModule.replace(*arrayType, returnType);
    else
        RELEASE_ASSERT_NOT_REACHED();
}

void EntryPointRewriter::constructInputStruct()
{
    // insert `var ${parameter.name()} = ${structName}.${parameter.name()}`
    AST::StructureMember::List structMembers;
    for (auto& parameter : borrow(m_parameters)) {
        structMembers.append(m_shaderModule.astBuilder().construct<AST::StructureMember>(
            SourceSpan::empty(),
            WTF::move(parameter.name),
            parameter.type,
            WTF::move(parameter.attributes)
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
        WTF::move(structMembers),
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
            WTF::move(lhs),
            AST::Identifier::make(path[i++])
        );
    }
    path.removeLast();
    m_materializations.append(m_shaderModule.astBuilder().construct<AST::AssignmentStatement>(
        SourceSpan::empty(),
        WTF::move(lhs),
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
        if (auto* builtinAttribute = dynamicDowncast<AST::BuiltinAttribute>(attribute)) {
            builtin = builtinAttribute->builtin();
            break;
        }
    }

    if (builtin.has_value()) {
        if (*builtin == Builtin::WorkgroupIndex) {
            m_usesWorkgroupIndex = true;
            String builtinName = "__wgslWorkgroupIndex"_s;
            materialize(path, data, IsBuiltin::Yes, &builtinName);
            return;
        }
        if (*builtin == Builtin::GlobalInvocationIndex) {
            m_usesWorkgroupIndex = true;
            m_usesGlobalInvocationIndex = true;
            String builtinName = "__wgslGlobalInvocationIndex"_s;
            materialize(path, data, IsBuiltin::Yes, &builtinName);
            return;
        }
        if (!path.isEmpty()) {
            // builtin was hoisted from a struct into a parameter, we need to reconstruct the struct
            // ${path}.${data.name} = __builtin${builtinID}
            // Note that we don't use ${data.name} on the right-hand side because it's the name of a
            // struct field, and it might not be unique.
            auto builtinName = makeString("__builtin"_s, String::number(m_builtinID++));
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
    m_parameters.append(WTF::move(data));
}

void EntryPointRewriter::appendBuiltins()
{
    if (m_usesWorkgroupIndex || m_usesGlobalInvocationIndex) {
        auto ensureBuiltin = [&](Builtin targetBuiltin, ASCIILiteral defaultName, const Type* paramType) -> String {
            for (auto& existing : borrow(m_builtins)) {
                if (existing.builtin == targetBuiltin)
                    return existing.name.id();
            }
            auto& typeExpr = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(defaultName));
            typeExpr.m_inferredType = paramType;
            auto& builtinAttr = m_shaderModule.astBuilder().construct<AST::BuiltinAttribute>(SourceSpan::empty(), targetBuiltin);
            m_builtins.append({
                {
                    AST::Identifier::make(defaultName),
                    typeExpr,
                    AST::Attribute::List { builtinAttr }
                },
                targetBuiltin
            });
            return defaultName;
        };

        auto* u32Type = m_shaderModule.types().u32Type();
        auto* vec3uType = m_shaderModule.types().vectorType(3, u32Type);
        String wgIdName = ensureBuiltin(Builtin::WorkgroupId, "__wgslWorkgroupId"_s, vec3uType);
        String numWgName = ensureBuiltin(Builtin::NumWorkgroups, "__wgslNumWorkgroups"_s, vec3uType);

        auto makeVecComponent = [&](const String& vecName, ASCIILiteral comp) -> AST::Expression& {
            auto& base = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(vecName));
            base.m_inferredType = vec3uType;
            auto& access = m_shaderModule.astBuilder().construct<AST::FieldAccessExpression>(SourceSpan::empty(), base, AST::Identifier::make(comp));
            access.m_inferredType = u32Type;
            return access;
        };
        auto makeBinary = [&](AST::Expression& lhs, AST::Expression& rhs, AST::BinaryOperation op) -> AST::Expression& {
            auto& bin = m_shaderModule.astBuilder().construct<AST::BinaryExpression>(SourceSpan::empty(), lhs, rhs, op);
            bin.m_inferredType = u32Type;
            return bin;
        };

        // ((wgId.z * numWg.y + wgId.y) * numWg.x) + wgId.x
        auto& zMulY = makeBinary(makeVecComponent(wgIdName, "z"_s), makeVecComponent(numWgName, "y"_s), AST::BinaryOperation::Multiply);
        auto& plusY = makeBinary(zMulY, makeVecComponent(wgIdName, "y"_s), AST::BinaryOperation::Add);
        auto& mulX = makeBinary(plusY, makeVecComponent(numWgName, "x"_s), AST::BinaryOperation::Multiply);
        auto& wgIndexExpr = makeBinary(mulX, makeVecComponent(wgIdName, "x"_s), AST::BinaryOperation::Add);

        auto& u32TypeExpr1 = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("u32"_s));
        u32TypeExpr1.m_inferredType = u32Type;
        auto& wgIndexVar = m_shaderModule.astBuilder().construct<AST::Variable>(
            SourceSpan::empty(),
            AST::VariableFlavor::Var,
            AST::Identifier::make("__wgslWorkgroupIndex"_s),
            nullptr,
            &u32TypeExpr1,
            &wgIndexExpr,
            AST::Attribute::List { }
        );
        auto& wgIndexStmt = m_shaderModule.astBuilder().construct<AST::VariableStatement>(SourceSpan::empty(), wgIndexVar);

        AST::Statement::List linearIndexStmts;
        linearIndexStmts.append(wgIndexStmt);

        if (m_usesGlobalInvocationIndex) {
            String gidName = ensureBuiltin(Builtin::GlobalInvocationId, "__wgslGlobalInvocationId"_s, vec3uType);
            String tptgName = ensureBuiltin(Builtin::GlobalInvocationIndex, "__wgslThreadsPerThreadgroup"_s, vec3uType);

            // ((gid.z * (numWg.y * tptg.y) + gid.y) * (numWg.x * tptg.x)) + gid.x
            auto& gridSizeX = makeBinary(makeVecComponent(numWgName, "x"_s), makeVecComponent(tptgName, "x"_s), AST::BinaryOperation::Multiply);
            auto& gridSizeY = makeBinary(makeVecComponent(numWgName, "y"_s), makeVecComponent(tptgName, "y"_s), AST::BinaryOperation::Multiply);
            auto& gidZMulGridY = makeBinary(makeVecComponent(gidName, "z"_s), gridSizeY, AST::BinaryOperation::Multiply);
            auto& plusGidY = makeBinary(gidZMulGridY, makeVecComponent(gidName, "y"_s), AST::BinaryOperation::Add);
            auto& mulGridX = makeBinary(plusGidY, gridSizeX, AST::BinaryOperation::Multiply);
            auto& globalIndexExpr = makeBinary(mulGridX, makeVecComponent(gidName, "x"_s), AST::BinaryOperation::Add);

            auto& u32TypeExpr2 = m_shaderModule.astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("u32"_s));
            u32TypeExpr2.m_inferredType = u32Type;
            auto& globalIndexVar = m_shaderModule.astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make("__wgslGlobalInvocationIndex"_s),
                nullptr,
                &u32TypeExpr2,
                &globalIndexExpr,
                AST::Attribute::List { }
            );
            auto& globalIndexStmt = m_shaderModule.astBuilder().construct<AST::VariableStatement>(SourceSpan::empty(), globalIndexVar);
            linearIndexStmts.append(globalIndexStmt);
        }

        m_materializations.insertVector(0, linearIndexStmts);
    }

    for (auto& data : borrow(m_builtins)) {
        auto& parameter = m_shaderModule.astBuilder().construct<AST::Parameter>(
            SourceSpan::empty(),
            AST::Identifier::make(data.name),
            data.type,
            WTF::move(data.attributes),
            AST::ParameterRole::UserDefined
        );
        parameter.m_builtin = data.builtin;
        m_shaderModule.append(m_function.parameters(), parameter);
    }
}

void rewriteEntryPoints(ShaderModule& shaderModule, const HashMap<String, PipelineLayout*>& pipelineLayouts)
{
    for (auto& entryPoint : shaderModule.callGraph().entrypoints()) {
        if (!pipelineLayouts.contains(entryPoint.originalName))
            continue;
        EntryPointRewriter rewriter(shaderModule, entryPoint.function, entryPoint.stage);
        rewriter.rewrite();
    }
}

} // namespace WGSL
