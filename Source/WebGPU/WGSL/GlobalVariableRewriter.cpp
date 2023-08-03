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
#include "GlobalVariableRewriter.h"

#include "AST.h"
#include "ASTIdentifier.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "WGSL.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>

namespace WGSL {

constexpr bool shouldLogGlobalVariableRewriting = false;

class RewriteGlobalVariables : public AST::Visitor {
public:
    RewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, PrepareResult& result)
        : AST::Visitor()
        , m_callGraph(callGraph)
        , m_result(result)
    {
        UNUSED_PARAM(pipelineLayouts);
    }

    void run();

    void visit(AST::Function&) override;
    void visit(AST::Variable&) override;

    void visit(AST::CompoundStatement&) override;
    void visit(AST::AssignmentStatement&) override;

    void visit(AST::Expression&) override;

private:
    enum class Context : uint8_t { Local, Global };

    struct Global {
        struct Resource {
            unsigned group;
            unsigned binding;
        };

        std::optional<Resource> resource;
        AST::Variable* declaration;
    };

    template<typename Value>
    using IndexMap = HashMap<unsigned, Value, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    using UsedResources = IndexMap<IndexMap<Global*>>;
    using UsedPrivateGlobals = Vector<Global*>;

    struct UsedGlobals {
        UsedResources resources;
        UsedPrivateGlobals privateGlobals;
    };

    struct Insertion {
        AST::Statement* statement;
        unsigned index;
    };

    static AST::Identifier argumentBufferParameterName(unsigned group);
    static AST::Identifier argumentBufferStructName(unsigned group);

    void def(const AST::Identifier&, AST::Variable*);

    void collectGlobals();
    void visitEntryPoint(AST::Function&, AST::StageAttribute::Stage, PipelineLayout&);
    void visitCallee(const CallGraph::Callee&);
    UsedGlobals determineUsedGlobals(PipelineLayout&, AST::StageAttribute::Stage);
    void usesOverride(AST::Variable&);
    void insertStructs(const UsedResources&);
    void insertParameters(AST::Function&, const UsedResources&);
    void insertMaterializations(AST::Function&, const UsedResources&);
    void insertLocalDefinitions(AST::Function&, const UsedPrivateGlobals&);
    void readVariable(AST::IdentifierExpression&, AST::Variable&, Context);
    void insertBeforeCurrentStatement(AST::Statement&);

    void packResource(AST::Variable&);
    void packArrayResource(AST::Variable&, const Types::Array*);
    void packStructResource(AST::Variable&, const Types::Struct*);
    const Type* packStructType(const Types::Struct*);
    void updateReference(AST::Variable&, AST::TypeName&);

    enum Packing : uint8_t {
        Packed   = 1 << 0,
        Unpacked = 1 << 1,
        Either   = Packed | Unpacked,
    };

    Packing pack(Packing, AST::Expression&);
    Packing getPacking(AST::IdentifierExpression&);
    Packing getPacking(AST::FieldAccessExpression&);
    Packing getPacking(AST::IndexAccessExpression&);
    Packing getPacking(AST::BinaryExpression&);
    Packing getPacking(AST::UnaryExpression&);
    Packing getPacking(AST::CallExpression&);
    Packing packingForType(const Type*);

    CallGraph& m_callGraph;
    PrepareResult& m_result;
    HashMap<String, Global> m_globals;
    IndexMap<Vector<std::pair<unsigned, String>>> m_groupBindingMap;
    IndexMap<const Type*> m_structTypes;
    HashMap<String, AST::Variable*> m_defs;
    HashSet<String> m_reads;
    HashMap<AST::Function*, HashSet<String>> m_visitedFunctions;
    Reflection::EntryPointInformation* m_entryPointInformation { nullptr };
    unsigned m_constantId { 0 };
    unsigned m_currentStatementIndex { 0 };
    Vector<Insertion> m_pendingInsertions;
    HashMap<const Types::Struct*, const Type*> m_packedStructTypes;
};

void RewriteGlobalVariables::run()
{
    dataLogLnIf(shouldLogGlobalVariableRewriting, "BEGIN: GlobalVariableRewriter");

    collectGlobals();
    for (auto& entryPoint : m_callGraph.entrypoints()) {
        PipelineLayout pipelineLayout;
        auto it = m_result.entryPoints.find(entryPoint.function.name());
        RELEASE_ASSERT(it != m_result.entryPoints.end());
        m_entryPointInformation = &it->value;

        visitEntryPoint(entryPoint.function, entryPoint.stage, pipelineLayout);

        m_entryPointInformation->defaultLayout = WTFMove(pipelineLayout);
    }

    dataLogLnIf(shouldLogGlobalVariableRewriting, "END: GlobalVariableRewriter");
}

void RewriteGlobalVariables::visitCallee(const CallGraph::Callee& callee)
{
    const auto& updateCallee = [&] {
        for (auto& read : m_reads) {
            auto it = m_globals.find(read);
            RELEASE_ASSERT(it != m_globals.end());
            auto& global = it->value;
            m_callGraph.ast().append(callee.target->parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
                SourceSpan::empty(),
                AST::Identifier::make(read),
                *global.declaration->maybeReferenceType(),
                AST::Attribute::List { },
                AST::ParameterRole::UserDefined
            ));
        }
    };

    const auto& updateCallSites = [&] {
        for (auto& read : m_reads) {
            for (auto& call : callee.callSites) {
                m_callGraph.ast().append(call->arguments(), m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
                    SourceSpan::empty(),
                    AST::Identifier::make(read)
                ));
            }
        }
    };

    auto it = m_visitedFunctions.find(callee.target);
    if (it != m_visitedFunctions.end()) {
        dataLogLnIf(shouldLogGlobalVariableRewriting, "> Already visited callee: ", callee.target->name());
        m_reads = it->value;
        updateCallSites();
        return;
    }

    dataLogLnIf(shouldLogGlobalVariableRewriting, "> Visiting callee: ", callee.target->name());

    visit(*callee.target);
    updateCallee();
    updateCallSites();

    m_visitedFunctions.add(callee.target, m_reads);
}

void RewriteGlobalVariables::visit(AST::Function& function)
{
    HashSet<String> reads;
    for (auto& callee : m_callGraph.callees(function)) {
        visitCallee(callee);
        reads.formUnion(WTFMove(m_reads));
    }
    m_reads = WTFMove(reads);
    m_defs.clear();

    for (auto& parameter : function.parameters())
        def(parameter.name(), nullptr);

    // FIXME: detect when we shadow a global that a callee needs
    visit(function.body());
}

void RewriteGlobalVariables::visit(AST::Variable& variable)
{
    def(variable.name(), &variable);
    AST::Visitor::visit(variable);
}

void RewriteGlobalVariables::visit(AST::CompoundStatement& statement)
{
    auto indexScope = SetForScope(m_currentStatementIndex, 0);
    auto insertionScope = SetForScope(m_pendingInsertions, Vector<Insertion>());

    for (auto& statement : statement.statements()) {
        AST::Visitor::visit(statement);
        ++m_currentStatementIndex;
    }

    unsigned offset = 0;
    for (auto& insertion : m_pendingInsertions) {
        m_callGraph.ast().insert(statement.statements(), insertion.index + offset, AST::Statement::Ref(*insertion.statement));
        ++offset;
    }
}

void RewriteGlobalVariables::visit(AST::AssignmentStatement& statement)
{
    Packing lhsPacking = pack(Packing::Either, statement.lhs());
    ASSERT(lhsPacking != Packing::Either);
    Packing rhsPacking = pack(lhsPacking, statement.rhs());
    ASSERT_UNUSED(rhsPacking, lhsPacking == rhsPacking);
}

void RewriteGlobalVariables::visit(AST::Expression& expression)
{
    pack(Packing::Unpacked, expression);
}

auto RewriteGlobalVariables::pack(Packing expectedPacking, AST::Expression& expression) -> Packing
{
    const auto& visitAndReplace = [&](auto& expression) -> Packing {
        auto packing = getPacking(expression);
        if (expectedPacking & packing)
            return packing;

        auto* type = expression.inferredType();
        if (auto* referenceType = std::get_if<Types::Reference>(type))
            type = referenceType->element;
        ASCIILiteral operation;
        if (std::holds_alternative<Types::Struct>(*type))
            operation = packing == Packing::Packed ? "__unpack"_s : "__pack"_s;
        else if (std::holds_alternative<Types::Array>(*type)) {
            if (packing == Packing::Packed) {
                operation = "__unpack_array"_s;
                m_callGraph.ast().setUsesUnpackArray();
            } else {
                operation = "__pack_array"_s;
                m_callGraph.ast().setUsesPackArray();
            }
        } else {
            ASSERT(std::holds_alternative<Types::Vector>(*type));
            auto& vector = std::get<Types::Vector>(*type);
            ASSERT(std::holds_alternative<Types::Primitive>(*vector.element));
            switch (std::get<Types::Primitive>(*vector.element).kind) {
            case Types::Primitive::AbstractInt:
            case Types::Primitive::I32:
                operation = packing == Packing::Packed ? "int3"_s : "packed_int3"_s;
                break;
            case Types::Primitive::U32:
                operation = packing == Packing::Packed ? "uint3"_s : "packed_uint3"_s;
                break;
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                operation = packing == Packing::Packed ? "float3"_s : "packed_float3"_s;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        RELEASE_ASSERT(!operation.isNull());
        auto& callee = m_callGraph.ast().astBuilder().construct<AST::NamedTypeName>(
            SourceSpan::empty(),
            AST::Identifier::make(operation)
        );
        callee.m_resolvedType = m_callGraph.ast().types().bottomType();
        auto& argument = m_callGraph.ast().astBuilder().construct<std::remove_cvref_t<decltype(expression)>>(expression);
        auto& call = m_callGraph.ast().astBuilder().construct<AST::CallExpression>(
            SourceSpan::empty(),
            callee,
            AST::Expression::List { argument }
        );
        call.m_inferredType = argument.inferredType();
        m_callGraph.ast().replace(expression, call);
        return static_cast<Packing>(Packing::Either ^ packing);
    };

    switch (expression.kind()) {
    case AST::NodeKind::IdentifierExpression:
        return visitAndReplace(downcast<AST::IdentifierExpression>(expression));
    case AST::NodeKind::FieldAccessExpression:
        return visitAndReplace(downcast<AST::FieldAccessExpression>(expression));
    case AST::NodeKind::IndexAccessExpression:
        return visitAndReplace(downcast<AST::IndexAccessExpression>(expression));
    case AST::NodeKind::BinaryExpression:
        return visitAndReplace(downcast<AST::BinaryExpression>(expression));
    case AST::NodeKind::UnaryExpression:
        return visitAndReplace(downcast<AST::UnaryExpression>(expression));
    case AST::NodeKind::CallExpression:
        return visitAndReplace(downcast<AST::CallExpression>(expression));
    default:
        AST::Visitor::visit(expression);
        return Packing::Unpacked;
    }
}

auto RewriteGlobalVariables::getPacking(AST::IdentifierExpression& identifier) -> Packing
{
    auto packing = Packing::Unpacked;

    auto def = m_defs.find(identifier.identifier());
    if (def != m_defs.end()) {
        if (def->value)
            readVariable(identifier, *def->value, Context::Local);
        return packing;
    }

    auto it = m_globals.find(identifier.identifier());
    if (it == m_globals.end())
        return packing;
    readVariable(identifier, *it->value.declaration, Context::Global);

    if (it->value.resource.has_value())
        return packingForType(identifier.inferredType());

    return packing;
}

auto RewriteGlobalVariables::getPacking(AST::FieldAccessExpression& expression) -> Packing
{
    auto basePacking = pack(Packing::Either, expression.base());
    if (basePacking & Packing::Unpacked)
        return Packing::Unpacked;
    auto* baseType = expression.base().inferredType();
    if (auto* referenceType = std::get_if<Types::Reference>(baseType))
        baseType = referenceType->element;
    if (std::holds_alternative<Types::Vector>(*baseType))
        return Packing::Unpacked;
    ASSERT(std::holds_alternative<Types::Struct>(*baseType));
    auto& structType = std::get<Types::Struct>(*baseType);
    auto* fieldType = structType.fields.get(expression.fieldName());
    return packingForType(fieldType);
}

auto RewriteGlobalVariables::getPacking(AST::IndexAccessExpression& expression) -> Packing
{
    auto basePacking = pack(Packing::Either, expression.base());
    if (basePacking & Packing::Unpacked)
        return Packing::Unpacked;
    auto* baseType = expression.base().inferredType();
    if (auto* referenceType = std::get_if<Types::Reference>(baseType))
        baseType = referenceType->element;
    if (std::holds_alternative<Types::Vector>(*baseType))
        return Packing::Unpacked;
    ASSERT(std::holds_alternative<Types::Array>(*baseType));
    auto& arrayType = std::get<Types::Array>(*baseType);
    return packingForType(arrayType.element);
}

auto RewriteGlobalVariables::getPacking(AST::BinaryExpression& expression) -> Packing
{
    pack(Packing::Unpacked, expression.leftExpression());
    pack(Packing::Unpacked, expression.rightExpression());
    return Packing::Unpacked;
}

auto RewriteGlobalVariables::getPacking(AST::UnaryExpression& expression) -> Packing
{
    pack(Packing::Unpacked, expression.expression());
    return Packing::Unpacked;
}

auto RewriteGlobalVariables::getPacking(AST::CallExpression& call) -> Packing
{
    for (auto& argument : call.arguments())
        pack(Packing::Unpacked, argument);
    return Packing::Unpacked;
}

auto RewriteGlobalVariables::packingForType(const Type* type) -> Packing
{
    if (auto* referenceType = std::get_if<Types::Reference>(type))
        return packingForType(referenceType->element);

    if (auto* structType = std::get_if<Types::Struct>(type)) {
        if (structType->structure.role() == AST::StructureRole::UserDefinedResource)
            return Packing::Packed;
    } else if (auto* vectorType = std::get_if<Types::Vector>(type)) {
        if (vectorType->size == 3)
            return Packing::Packed;
    } else if (auto* arrayType = std::get_if<Types::Array>(type))
        return packingForType(arrayType->element);

    return Packing::Unpacked;
}

void RewriteGlobalVariables::collectGlobals()
{
    auto& globalVars = m_callGraph.ast().variables();
    for (auto& globalVar : globalVars) {
        std::optional<unsigned> group;
        std::optional<unsigned> binding;
        for (auto& attribute : globalVar.attributes()) {
            if (is<AST::GroupAttribute>(attribute)) {
                group = { *AST::extractInteger(downcast<AST::GroupAttribute>(attribute).group()) };
                continue;
            }
            if (is<AST::BindingAttribute>(attribute)) {
                binding = { *AST::extractInteger(downcast<AST::BindingAttribute>(attribute).binding()) };
                continue;
            }
        }

        std::optional<Global::Resource> resource;
        if (group.has_value()) {
            RELEASE_ASSERT(binding.has_value());
            resource = { *group, *binding };
        }

        dataLogLnIf(shouldLogGlobalVariableRewriting, "> Found global: ", globalVar.name(), ", isResource: ", resource.has_value() ? "yes" : "no");

        auto result = m_globals.add(globalVar.name(), Global {
            resource,
            &globalVar
        });
        ASSERT_UNUSED(result, result.isNewEntry);

        if (resource.has_value()) {
            auto result = m_groupBindingMap.add(resource->group, Vector<std::pair<unsigned, String>>());
            result.iterator->value.append({ resource->binding, globalVar.name() });
            packResource(globalVar);
        }
    }
}

void RewriteGlobalVariables::packResource(AST::Variable& global)
{
    auto* maybeTypeName = global.maybeTypeName();
    ASSERT(maybeTypeName);

    auto* resolvedType = maybeTypeName->resolvedType();
    if (auto* arrayType = std::get_if<Types::Array>(resolvedType)) {
        packArrayResource(global, arrayType);
        return;
    }

    if (auto* structType = std::get_if<Types::Struct>(resolvedType)) {
        packStructResource(global, structType);
        return;
    }
}

void RewriteGlobalVariables::packStructResource(AST::Variable& global, const Types::Struct* structType)
{
    const Type* packedStructType = packStructType(structType);
    auto& packedType = m_callGraph.ast().astBuilder().construct<AST::NamedTypeName>(
        SourceSpan::empty(),
        AST::Identifier::make(std::get<Types::Struct>(*packedStructType).structure.name().id())
    );
    packedType.m_resolvedType = packedStructType;
    auto& namedTypeName = downcast<AST::NamedTypeName>(*global.maybeTypeName());
    m_callGraph.ast().replace(namedTypeName, packedType);
    updateReference(global, packedType);
}

void RewriteGlobalVariables::packArrayResource(AST::Variable& global, const Types::Array* arrayType)
{
    auto* structType = std::get_if<Types::Struct>(arrayType->element);
    if (!structType)
        return;

    const Type* packedStructType = packStructType(structType);
    auto& packedType = m_callGraph.ast().astBuilder().construct<AST::NamedTypeName>(
        SourceSpan::empty(),
        AST::Identifier::make(std::get<Types::Struct>(*packedStructType).structure.name().id())
    );
    packedType.m_resolvedType = packedStructType;

    auto& arrayTypeName = downcast<AST::ArrayTypeName>(*global.maybeTypeName());
    auto& packedArrayTypeName = m_callGraph.ast().astBuilder().construct<AST::ArrayTypeName>(
        arrayTypeName.span(),
        &packedType,
        arrayTypeName.maybeElementCount()
    );
    packedArrayTypeName.m_resolvedType = m_callGraph.ast().types().arrayType(packedStructType, arrayType->size);

    m_callGraph.ast().replace(arrayTypeName, packedArrayTypeName);
    updateReference(global, packedArrayTypeName);
}

void RewriteGlobalVariables::updateReference(AST::Variable& global, AST::TypeName& packedType)
{
    auto* maybeReference = global.maybeReferenceType();
    ASSERT(maybeReference);
    ASSERT(is<AST::ReferenceTypeName>(*maybeReference));
    auto& reference = downcast<AST::ReferenceTypeName>(*maybeReference);
    auto* referenceType = std::get_if<Types::Reference>(reference.resolvedType());
    ASSERT(referenceType);
    auto& packedTypeReference = m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeName>(
        SourceSpan::empty(),
        packedType
    );
    packedTypeReference.m_resolvedType = m_callGraph.ast().types().referenceType(
        referenceType->addressSpace,
        packedType.resolvedType(),
        referenceType->accessMode
    );
    m_callGraph.ast().replace(reference, packedTypeReference);
}

const Type* RewriteGlobalVariables::packStructType(const Types::Struct* structType)
{
    if (structType->structure.role() == AST::StructureRole::UserDefinedResource)
        return m_packedStructTypes.get(structType);

    ASSERT(structType->structure.role() == AST::StructureRole::UserDefined);
    m_callGraph.ast().replace(&structType->structure.role(), AST::StructureRole::UserDefinedResource);

    String packedStructName = makeString("__", structType->structure.name(), "_Packed");
    auto& packedStruct = m_callGraph.ast().astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        AST::Identifier::make(packedStructName),
        AST::StructureMember::List(structType->structure.members()),
        AST::Attribute::List { },
        AST::StructureRole::PackedResource,
        &structType->structure
    );
    m_callGraph.ast().append(m_callGraph.ast().structures(), packedStruct);
    const Type* packedStructType = m_callGraph.ast().types().structType(packedStruct);
    m_packedStructTypes.add(structType, packedStructType);
    return packedStructType;
}

void RewriteGlobalVariables::visitEntryPoint(AST::Function& function, AST::StageAttribute::Stage stage, PipelineLayout& pipelineLayout)
{
    m_reads.clear();
    m_structTypes.clear();

    dataLogLnIf(shouldLogGlobalVariableRewriting, "> Visiting entrypoint: ", function.name());

    visit(function);
    if (m_reads.isEmpty())
        return;

    auto usedGlobals = determineUsedGlobals(pipelineLayout, stage);
    insertStructs(usedGlobals.resources);
    insertParameters(function, usedGlobals.resources);
    insertMaterializations(function, usedGlobals.resources);
    insertLocalDefinitions(function, usedGlobals.privateGlobals);
}

static BindGroupLayoutEntry::BindingMember bindingMemberForGlobal(auto& global)
{
    auto* variable = global.declaration;
    ASSERT(variable);
    auto* maybeReference = variable->maybeReferenceType();
    auto* type = variable->storeType();
    ASSERT(type);
    auto addressSpace = [&]() {
        if (maybeReference) {
            auto& reference = downcast<AST::ReferenceTypeName>(*maybeReference);
            auto* referenceType = std::get_if<Types::Reference>(reference.resolvedType());
            if (referenceType && referenceType->addressSpace == AddressSpace::Storage)
                return BufferBindingType::Storage;
        }

        return BufferBindingType::Uniform;
    };

    using namespace WGSL::Types;
    return WTF::switchOn(*type, [&](const Primitive& primitive) -> BindGroupLayoutEntry::BindingMember {
        switch (primitive.kind) {
        case Types::Primitive::AbstractInt:
        case Types::Primitive::I32:
        case Types::Primitive::U32:
        case Types::Primitive::AbstractFloat:
        case Types::Primitive::F32:
        case Types::Primitive::Void:
        case Types::Primitive::Bool:
            return BufferBindingLayout {
                .type = addressSpace(),
                .hasDynamicOffset = false,
                .minBindingSize = 0
            };
        case Types::Primitive::Sampler:
            return SamplerBindingLayout {
                .type = SamplerBindingType::Filtering
            };
        case Types::Primitive::TextureExternal:
            return ExternalTextureBindingLayout { };
        }
    }, [&](const Vector& vector) -> BindGroupLayoutEntry::BindingMember {
        auto* primitive = std::get_if<Primitive>(vector.element);
        UNUSED_PARAM(primitive);
        return BufferBindingLayout {
            .type = addressSpace(),
            .hasDynamicOffset = false,
            .minBindingSize = 0
        };
    }, [&](const Matrix& matrix) -> BindGroupLayoutEntry::BindingMember {
        UNUSED_PARAM(matrix);
        return BufferBindingLayout {
            .type = addressSpace(),
            .hasDynamicOffset = false,
            .minBindingSize = 0
        };
    }, [&](const Array& array) -> BindGroupLayoutEntry::BindingMember {
        UNUSED_PARAM(array);
        return BufferBindingLayout {
            .type = addressSpace(),
            .hasDynamicOffset = false,
            .minBindingSize = 0
        };
    }, [&](const Struct& structure) -> BindGroupLayoutEntry::BindingMember {
        UNUSED_PARAM(structure);
        return BufferBindingLayout {
            .type = addressSpace(),
            .hasDynamicOffset = false,
            .minBindingSize = 0
        };
    }, [&](const Texture& texture) -> BindGroupLayoutEntry::BindingMember {
        TextureViewDimension viewDimension;
        bool multisampled = false;
        bool isStorageTexture = false;
        switch (texture.kind) {
        case Types::Texture::Kind::Texture1d:
            viewDimension = TextureViewDimension::OneDimensional;
            break;
        case Types::Texture::Kind::Texture2d:
            viewDimension = TextureViewDimension::TwoDimensional;
            break;
        case Types::Texture::Kind::Texture2dArray:
            viewDimension = TextureViewDimension::TwoDimensionalArray;
            break;
        case Types::Texture::Kind::Texture3d:
            viewDimension = TextureViewDimension::ThreeDimensional;
            break;
        case Types::Texture::Kind::TextureCube:
            viewDimension = TextureViewDimension::Cube;
            break;
        case Types::Texture::Kind::TextureCubeArray:
            viewDimension = TextureViewDimension::CubeArray;
            break;
        case Types::Texture::Kind::TextureMultisampled2d:
            viewDimension = TextureViewDimension::TwoDimensional;
            multisampled = true;
            break;

        case Types::Texture::Kind::TextureStorage1d:
            isStorageTexture = true;
            viewDimension = TextureViewDimension::OneDimensional;
            break;
        case Types::Texture::Kind::TextureStorage2d:
            isStorageTexture = true;
            viewDimension = TextureViewDimension::TwoDimensional;
            break;
        case Types::Texture::Kind::TextureStorage2dArray:
            isStorageTexture = true;
            viewDimension = TextureViewDimension::TwoDimensionalArray;
            break;
        case Types::Texture::Kind::TextureStorage3d:
            isStorageTexture = true;
            viewDimension = TextureViewDimension::ThreeDimensional;
            break;
        }

        if (isStorageTexture) {
            return StorageTextureBindingLayout {
                .viewDimension = viewDimension
            };
        }

        return TextureBindingLayout {
            .sampleType = TextureSampleType::Float,
            .viewDimension = viewDimension,
            .multisampled = multisampled
        };
    }, [&](const Reference&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const Function&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const Bottom&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    });
}

auto RewriteGlobalVariables::determineUsedGlobals(PipelineLayout& pipelineLayout, AST::StageAttribute::Stage stage) -> UsedGlobals
{
    UsedGlobals usedGlobals;
    for (const auto& globalName : m_reads) {
        auto it = m_globals.find(globalName);
        RELEASE_ASSERT(it != m_globals.end());
        auto& global = it->value;
        AST::Variable& variable = *global.declaration;
        switch (variable.flavor()) {
        case AST::VariableFlavor::Override:
            usesOverride(variable);
            break;
        case AST::VariableFlavor::Var:
        case AST::VariableFlavor::Let:
        case AST::VariableFlavor::Const:
            if (!global.resource.has_value()) {
                usedGlobals.privateGlobals.append(&global);
                continue;
            }
            break;
        }

        auto group = global.resource->group;
        auto result = usedGlobals.resources.add(group, IndexMap<Global*>());
        result.iterator->value.add(global.resource->binding, &global);

        if (pipelineLayout.bindGroupLayouts.size() <= group)
            pipelineLayout.bindGroupLayouts.grow(group + 1);

        ShaderStage shaderStage;
        switch (stage) {
        case AST::StageAttribute::Stage::Compute:
            shaderStage = ShaderStage::Compute;
            break;
        case AST::StageAttribute::Stage::Vertex:
            shaderStage = ShaderStage::Vertex;
            break;
        case AST::StageAttribute::Stage::Fragment:
            shaderStage = ShaderStage::Fragment;
            break;
        }
        pipelineLayout.bindGroupLayouts[group].entries.append({
            .binding = global.resource->binding,
            .visibility = shaderStage,
            .bindingMember = bindingMemberForGlobal(global)
        });
    }
    return usedGlobals;
}

void RewriteGlobalVariables::usesOverride(AST::Variable& variable)
{
    Reflection::SpecializationConstantType constantType;
    const Type* type = variable.storeType();
    ASSERT(std::holds_alternative<Types::Primitive>(*type));
    const auto& primitive = std::get<Types::Primitive>(*type);
    switch (primitive.kind) {
    case Types::Primitive::Bool:
        constantType = Reflection::SpecializationConstantType::Boolean;
        break;
    case Types::Primitive::F32:
        constantType = Reflection::SpecializationConstantType::Float;
        break;
    case Types::Primitive::I32:
        constantType = Reflection::SpecializationConstantType::Int;
        break;
    case Types::Primitive::U32:
        constantType = Reflection::SpecializationConstantType::Unsigned;
        break;
    case Types::Primitive::Void:
    case Types::Primitive::AbstractInt:
    case Types::Primitive::AbstractFloat:
    case Types::Primitive::Sampler:
    case Types::Primitive::TextureExternal:
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_entryPointInformation->specializationConstants.add(variable.name(), Reflection::SpecializationConstant { String(), constantType });
}

void RewriteGlobalVariables::insertStructs(const UsedResources& usedResources)
{
    for (auto& groupBinding : m_groupBindingMap) {
        unsigned group = groupBinding.key;

        auto usedResource = usedResources.find(group);
        if (usedResource == usedResources.end())
            continue;

        const auto& bindingGlobalMap = groupBinding.value;
        const IndexMap<Global*>& usedBindings = usedResource->value;

        AST::Identifier structName = argumentBufferStructName(group);
        AST::StructureMember::List structMembers;

        for (auto [binding, globalName] : bindingGlobalMap) {
            if (!usedBindings.contains(binding))
                continue;

            auto it = m_globals.find(globalName);
            RELEASE_ASSERT(it != m_globals.end());
            auto& global = it->value;
            ASSERT(global.declaration->maybeTypeName());
            auto span = global.declaration->span();
            structMembers.append(m_callGraph.ast().astBuilder().construct<AST::StructureMember>(
                span,
                AST::Identifier::make(global.declaration->name()),
                *global.declaration->maybeReferenceType(),
                AST::Attribute::List {
                    m_callGraph.ast().astBuilder().construct<AST::BindingAttribute>(
                        span,
                        m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(span, binding)
                    )
                }
            ));
        }

        m_callGraph.ast().append(m_callGraph.ast().structures(), m_callGraph.ast().astBuilder().construct<AST::Structure>(
            SourceSpan::empty(),
            WTFMove(structName),
            WTFMove(structMembers),
            AST::Attribute::List { },
            AST::StructureRole::BindGroup
        ));
        m_structTypes.add(groupBinding.key, m_callGraph.ast().types().structType(m_callGraph.ast().structures().last()));
    }
}

void RewriteGlobalVariables::insertParameters(AST::Function& function, const UsedResources& usedResources)
{
    auto span = function.span();
    for (auto& it : usedResources) {
        unsigned group = it.key;
        auto& type = m_callGraph.ast().astBuilder().construct<AST::NamedTypeName>(span, argumentBufferStructName(group));
        type.m_resolvedType = m_structTypes.get(group);
        m_callGraph.ast().append(function.parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
            span,
            argumentBufferParameterName(group),
            type,
            AST::Attribute::List {
                m_callGraph.ast().astBuilder().construct<AST::GroupAttribute>(
                    span,
                    m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(span, group)
                )
            },
            AST::ParameterRole::BindGroup
        ));
    }
}

void RewriteGlobalVariables::insertMaterializations(AST::Function& function, const UsedResources& usedResources)
{
    auto span = function.span();
    for (auto& [group, bindings] : usedResources) {
        auto& argument = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
            span,
            AST::Identifier::make(argumentBufferParameterName(group))
        );

        for (auto& [_, global] : bindings) {
            auto& name = global->declaration->name();
            String fieldName = name;
            auto* storeType = global->declaration->storeType();
            if (isPrimitive(storeType, Types::Primitive::TextureExternal)) {
                fieldName = makeString("__", name);
                m_callGraph.ast().setUsesExternalTextures();
            }
            auto& access = m_callGraph.ast().astBuilder().construct<AST::FieldAccessExpression>(
                SourceSpan::empty(),
                argument,
                AST::Identifier::make(WTFMove(fieldName))
            );
            auto& variable = m_callGraph.ast().astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Let,
                AST::Identifier::make(name),
                nullptr,
                global->declaration->maybeReferenceType(),
                &access,
                AST::Attribute::List { }
            );
            auto& variableStatement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(
                SourceSpan::empty(),
                variable
            );
            m_callGraph.ast().insert(function.body().statements(), 0, AST::Statement::Ref(variableStatement));
        }
    }
}

void RewriteGlobalVariables::insertLocalDefinitions(AST::Function& function, const UsedPrivateGlobals& usedPrivateGlobals)
{
    for (auto* global : usedPrivateGlobals) {
        auto& variable = *global->declaration;
        auto& variableStatement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(SourceSpan::empty(), variable);
        m_callGraph.ast().insert(function.body().statements(), 0, std::reference_wrapper<AST::Statement>(variableStatement));
    }
}

void RewriteGlobalVariables::def(const AST::Identifier& name, AST::Variable* variable)
{
    dataLogLnIf(shouldLogGlobalVariableRewriting, "> def: ", name, " at line:", name.span().line, " column: ", name.span().lineOffset);
    m_defs.add(name, variable);
}

void RewriteGlobalVariables::readVariable(AST::IdentifierExpression& identifier, AST::Variable& variable, Context context)
{
    if (variable.flavor() != AST::VariableFlavor::Const) {
        if (context == Context::Global) {
            dataLogLnIf(shouldLogGlobalVariableRewriting, "> read global: ", identifier.identifier(), " at line:", identifier.span().line, " column: ", identifier.span().lineOffset);
            m_reads.add(identifier.identifier());
        }
        return;
    }

    String newName = makeString("__const", String::number(++m_constantId));
    auto& newInitializer = m_callGraph.ast().astBuilder().construct<AST::IdentityExpression>(
        variable.maybeInitializer()->span(),
        *variable.maybeInitializer()
    );
    newInitializer.m_inferredType = identifier.inferredType();
    auto& newVariable = m_callGraph.ast().astBuilder().construct<AST::Variable>(
        variable.span(),
        AST::VariableFlavor::Let,
        AST::Identifier::make(newName),
        nullptr,
        variable.maybeTypeName(),
        &newInitializer,
        AST::Attribute::List { }
    );

    m_callGraph.ast().replace(&identifier.identifier(), AST::Identifier::make(newName));

    auto& statement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(
        SourceSpan::empty(),
        newVariable
    );
    insertBeforeCurrentStatement(statement);
}

void RewriteGlobalVariables::insertBeforeCurrentStatement(AST::Statement& statement)
{
    m_pendingInsertions.append({ &statement, m_currentStatementIndex });
}

AST::Identifier RewriteGlobalVariables::argumentBufferParameterName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBufer_", String::number(group)));
}

AST::Identifier RewriteGlobalVariables::argumentBufferStructName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBuferT_", String::number(group)));
}

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, PrepareResult& result)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts, result).run();
}

} // namespace WGSL
