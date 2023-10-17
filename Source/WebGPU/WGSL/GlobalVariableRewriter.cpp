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
    RewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts)
        : AST::Visitor()
        , m_callGraph(callGraph)
        , m_pipelineLayouts(pipelineLayouts)
    {
    }

    void run();

    void visit(AST::Function&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Parameter&) override;

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
    void visitEntryPoint(const CallGraph::EntryPoint&);
    void visitCallee(const CallGraph::Callee&);
    UsedGlobals determineUsedGlobals();
    void usesOverride(AST::Variable&);
    Vector<unsigned> insertStructs(const UsedResources&);
    Vector<unsigned> insertStructs(const PipelineLayout&);
    AST::StructureMember& createArgumentBufferEntry(unsigned binding, AST::Variable&);
    AST::StructureMember& createArgumentBufferEntry(unsigned binding, const SourceSpan&, const String& name, AST::Expression& type);
    void finalizeArgumentBufferStruct(unsigned group, Vector<std::pair<unsigned, AST::StructureMember*>>&);
    void insertParameters(AST::Function&, const Vector<unsigned>&);
    void insertMaterializations(AST::Function&, const UsedResources&);
    void insertLocalDefinitions(AST::Function&, const UsedPrivateGlobals&);
    void readVariable(AST::IdentifierExpression&, AST::Variable&, Context);
    void insertBeforeCurrentStatement(AST::Statement&);
    AST::Expression& bufferLengthType();
    AST::Expression& bufferLengthReferenceType();

    void packResource(AST::Variable&);
    void packArrayResource(AST::Variable&, const Types::Array*);
    void packStructResource(AST::Variable&, const Types::Struct*);
    bool containsRuntimeArray(const Type*);
    const Type* packType(const Type*);
    const Type* packStructType(const Types::Struct*);
    const Type* packArrayType(const Types::Array*);
    void updateReference(AST::Variable&, AST::Expression&);

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
    HashMap<String, Global> m_globals;
    HashMap<std::tuple<unsigned, unsigned>, AST::Variable*> m_globalsByBinding;
    IndexMap<Vector<std::pair<unsigned, String>>> m_groupBindingMap;
    IndexMap<const Type*> m_structTypes;
    HashMap<String, AST::Variable*> m_defs;
    HashSet<String> m_reads;
    HashMap<AST::Function*, HashSet<String>> m_visitedFunctions;
    Reflection::EntryPointInformation* m_entryPointInformation { nullptr };
    PipelineLayout* m_generatedLayout { nullptr };
    unsigned m_constantId { 0 };
    unsigned m_currentStatementIndex { 0 };
    Vector<Insertion> m_pendingInsertions;
    HashMap<const Types::Struct*, const Type*> m_packedStructTypes;
    ShaderStage m_stage;
    const HashMap<String, std::optional<PipelineLayout>>& m_pipelineLayouts;
    HashMap<AST::Variable*, AST::Variable*> m_bufferLengthMap;
    AST::Expression* m_bufferLengthType { nullptr };
    AST::Expression* m_bufferLengthReferenceType { nullptr };
};

void RewriteGlobalVariables::run()
{
    dataLogLnIf(shouldLogGlobalVariableRewriting, "BEGIN: GlobalVariableRewriter");

    collectGlobals();
    for (auto& entryPoint : m_callGraph.entrypoints())
        visitEntryPoint(entryPoint);

    dataLogLnIf(shouldLogGlobalVariableRewriting, "END: GlobalVariableRewriter");
}

void RewriteGlobalVariables::visitCallee(const CallGraph::Callee& callee)
{
    const auto& updateCallee = [&] {
        for (auto& read : m_reads) {
            auto it = m_globals.find(read);
            RELEASE_ASSERT(it != m_globals.end());
            auto& global = it->value;
            AST::Expression* type;
            if (global.declaration->flavor() == AST::VariableFlavor::Var)
                type = global.declaration->maybeReferenceType();
            else {
                ASSERT(global.declaration->flavor() == AST::VariableFlavor::Override);
                type = global.declaration->maybeTypeName();
                if (!type) {
                    auto* storeType = global.declaration->storeType();
                    auto& typeExpression = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make(storeType->toString()));
                    typeExpression.m_inferredType = storeType;
                    type = &typeExpression;
                }
            }
            ASSERT(type);

            m_callGraph.ast().append(callee.target->parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
                SourceSpan::empty(),
                AST::Identifier::make(read),
                *type,
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

    def(function.name(), nullptr);
    AST::Visitor::visit(function);
}

void RewriteGlobalVariables::visit(AST::Parameter& parameter)
{
    def(parameter.name(), nullptr);
    AST::Visitor::visit(parameter);
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
                operation = "__unpack"_s;
                m_callGraph.ast().setUsesUnpackArray();
            } else {
                operation = "__pack"_s;
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
        auto& callee = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
            SourceSpan::empty(),
            AST::Identifier::make(operation)
        );
        callee.m_inferredType = m_callGraph.ast().types().bottomType();
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
    if (is<AST::IdentifierExpression>(call.target())) {
        auto& target = downcast<AST::IdentifierExpression>(call.target());
        if (target.identifier() == "arrayLength"_s) {
            ASSERT(call.arguments().size() == 1);
            const auto& getBase = [&](auto&& getBase, AST::Expression& expression) -> AST::Expression& {
                if (is<AST::IdentityExpression>(expression))
                    return getBase(getBase, downcast<AST::IdentityExpression>(expression).expression());
                if (is<AST::UnaryExpression>(expression))
                    return getBase(getBase, downcast<AST::UnaryExpression>(expression).expression());
                if (is<AST::FieldAccessExpression>(expression))
                    return getBase(getBase, downcast<AST::FieldAccessExpression>(expression).base());
                if (is<AST::IdentifierExpression>(expression))
                    return expression;
                RELEASE_ASSERT_NOT_REACHED();
            };
            auto& arrayPointer = call.arguments()[0];
            auto& base = getBase(getBase, arrayPointer);
            ASSERT(is<AST::IdentifierExpression>(base));
            auto& identifier = downcast<AST::IdentifierExpression>(base).identifier();
            ASSERT(m_globals.contains(identifier));
            auto lengthName = makeString("__", identifier, "_ArrayLength");
            auto& length = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
                SourceSpan::empty(),
                AST::Identifier::make(lengthName)
            );
            length.m_inferredType = m_callGraph.ast().types().u32Type();

            auto* arrayPointerType = arrayPointer.inferredType();
            ASSERT(std::holds_alternative<Types::Pointer>(*arrayPointerType));
            auto& arrayType = std::get<Types::Pointer>(*arrayPointerType).element;
            ASSERT(std::holds_alternative<Types::Array>(*arrayType));
            auto arrayStride = std::get<Types::Array>(*arrayType).element->size();

            auto& strideExpression = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
                SourceSpan::empty(),
                arrayStride
            );
            strideExpression.m_inferredType = m_callGraph.ast().types().u32Type();

            auto& elementCount = m_callGraph.ast().astBuilder().construct<AST::BinaryExpression>(
                SourceSpan::empty(),
                length,
                strideExpression,
                AST::BinaryOperation::Divide
            );
            elementCount.m_inferredType = m_callGraph.ast().types().u32Type();

            m_callGraph.ast().replace(call, elementCount);
            visit(base); // we also need to mark the array as read
            return getPacking(elementCount);
        }
    }

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
    Vector<std::tuple<AST::Variable*, unsigned>> bufferLengths;
    for (auto& globalVar : globalVars) {
        std::optional<unsigned> group;
        std::optional<unsigned> binding;
        for (auto& attribute : globalVar.attributes()) {
            if (is<AST::GroupAttribute>(attribute)) {
                group = downcast<AST::GroupAttribute>(attribute).group().constantValue()->toInt();
                continue;
            }
            if (is<AST::BindingAttribute>(attribute)) {
                binding = downcast<AST::BindingAttribute>(attribute).binding().constantValue()->toInt();
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
            m_globalsByBinding.add({ resource->group + 1, resource->binding + 1 }, &globalVar);

            auto result = m_groupBindingMap.add(resource->group, Vector<std::pair<unsigned, String>>());
            result.iterator->value.append({ resource->binding, globalVar.name() });
            packResource(globalVar);

            if (!m_generatedLayout || containsRuntimeArray(globalVar.maybeReferenceType()->inferredType()))
                bufferLengths.append({ &globalVar, *group });
        }
    }

    if (!bufferLengths.isEmpty()) {
        for (const auto& [variable, group] : bufferLengths) {
            auto name = AST::Identifier::make(makeString("__", variable->name(), "_ArrayLength"));
            auto& lengthVariable = m_callGraph.ast().astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Var,
                AST::Identifier::make(name),
                &bufferLengthType(),
                nullptr
            );
            lengthVariable.m_referenceType = &bufferLengthReferenceType();

            auto it = m_groupBindingMap.find(group);
            ASSERT(it != m_groupBindingMap.end());

            auto binding = it->value.last().first + 1;
            auto result = m_globals.add(name, Global {
                { {
                    group,
                    binding,
                } },
                &lengthVariable
            });
            m_bufferLengthMap.add(variable, &lengthVariable);
            ASSERT_UNUSED(result, result.isNewEntry);
            it->value.append({ binding, name });
        }
    }
}

AST::Expression& RewriteGlobalVariables::bufferLengthType()
{
    if (m_bufferLengthType)
        return *m_bufferLengthType;
    m_bufferLengthType = &m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("u32"_s));
    m_bufferLengthType->m_inferredType = m_callGraph.ast().types().u32Type();
    return *m_bufferLengthType;
}

AST::Expression& RewriteGlobalVariables::bufferLengthReferenceType()
{
    if (m_bufferLengthReferenceType)
        return *m_bufferLengthReferenceType;

    m_bufferLengthReferenceType = &m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeExpression>(
        SourceSpan::empty(),
        bufferLengthType()
    );
    m_bufferLengthReferenceType->m_inferredType = m_callGraph.ast().types().referenceType(AddressSpace::Handle, m_callGraph.ast().types().u32Type(), AccessMode::Read);
    return *m_bufferLengthReferenceType;
}

void RewriteGlobalVariables::packResource(AST::Variable& global)
{
    auto* maybeTypeName = global.maybeTypeName();
    ASSERT(maybeTypeName);

    auto* resolvedType = maybeTypeName->inferredType();
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
    if (!packedStructType)
        return;

    auto& packedType = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(std::get<Types::Struct>(*packedStructType).structure.name().id())
    );
    packedType.m_inferredType = packedStructType;
    auto& namedTypeName = downcast<AST::IdentifierExpression>(*global.maybeTypeName());
    m_callGraph.ast().replace(namedTypeName, packedType);
    updateReference(global, packedType);
}

void RewriteGlobalVariables::packArrayResource(AST::Variable& global, const Types::Array* arrayType)
{
    const Type* packedArrayType = packArrayType(arrayType);
    if (!packedArrayType)
        return;

    const Type* packedStructType = std::get<Types::Array>(*packedArrayType).element;
    auto& packedType = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(std::get<Types::Struct>(*packedStructType).structure.name().id())
    );
    packedType.m_inferredType = packedStructType;

    auto& arrayTypeName = downcast<AST::ArrayTypeExpression>(*global.maybeTypeName());
    auto& packedArrayTypeName = m_callGraph.ast().astBuilder().construct<AST::ArrayTypeExpression>(
        arrayTypeName.span(),
        &packedType,
        arrayTypeName.maybeElementCount()
    );
    packedArrayTypeName.m_inferredType = packedArrayType;

    m_callGraph.ast().replace(arrayTypeName, packedArrayTypeName);
    updateReference(global, packedArrayTypeName);
}

void RewriteGlobalVariables::updateReference(AST::Variable& global, AST::Expression& packedType)
{
    auto* maybeReference = global.maybeReferenceType();
    ASSERT(maybeReference);
    ASSERT(is<AST::ReferenceTypeExpression>(*maybeReference));
    auto& reference = downcast<AST::ReferenceTypeExpression>(*maybeReference);
    auto* referenceType = std::get_if<Types::Reference>(reference.inferredType());
    ASSERT(referenceType);
    auto& packedTypeReference = m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeExpression>(
        SourceSpan::empty(),
        packedType
    );
    packedTypeReference.m_inferredType = m_callGraph.ast().types().referenceType(
        referenceType->addressSpace,
        packedType.inferredType(),
        referenceType->accessMode
    );
    m_callGraph.ast().replace(reference, packedTypeReference);
}

bool RewriteGlobalVariables::containsRuntimeArray(const Type* type)
{
    if (auto* referenceType = std::get_if<Types::Reference>(type))
        return containsRuntimeArray(referenceType->element);
    if (auto* structType = std::get_if<Types::Struct>(type))
        return containsRuntimeArray(structType->structure.members().last().type().inferredType());
    if (auto* arrayType = std::get_if<Types::Array>(type))
        return !arrayType->size.has_value();
    return false;
}

const Type* RewriteGlobalVariables::packType(const Type* type)
{
    if (auto* structType = std::get_if<Types::Struct>(type))
        return packStructType(structType);
    if (auto* arrayType = std::get_if<Types::Array>(type))
        return packArrayType(arrayType);
    if (auto* vectorType = std::get_if<Types::Vector>(type)) {
        if (vectorType->size == 3)
            return type;
    }
    return nullptr;
}

const Type* RewriteGlobalVariables::packStructType(const Types::Struct* structType)
{
    if (structType->structure.role() == AST::StructureRole::UserDefinedResource)
        return m_packedStructTypes.get(structType);

    m_callGraph.ast().setUsesPackedStructs();

    // Ensure we pack nested structs
    bool packedAnyMember = false;
    for (auto& member : structType->structure.members()) {
        if (packType(member.type().inferredType()))
            packedAnyMember = true;
    }
    if (!packedAnyMember)
        return nullptr;

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

const Type* RewriteGlobalVariables::packArrayType(const Types::Array* arrayType)
{
    auto* structType = std::get_if<Types::Struct>(arrayType->element);
    if (!structType)
        return nullptr;

    const Type* packedStructType = packStructType(structType);
    if (!packedStructType)
        return nullptr;

    m_callGraph.ast().setUsesUnpackArray();
    m_callGraph.ast().setUsesPackArray();
    return m_callGraph.ast().types().arrayType(packedStructType, arrayType->size);
}

void RewriteGlobalVariables::visitEntryPoint(const CallGraph::EntryPoint& entryPoint)
{
    m_reads.clear();
    m_structTypes.clear();

    dataLogLnIf(shouldLogGlobalVariableRewriting, "> Visiting entrypoint: ", entryPoint.function.name());

    m_entryPointInformation = &entryPoint.information;

    auto it = m_pipelineLayouts.find(m_entryPointInformation->originalName);
    ASSERT(it != m_pipelineLayouts.end());

    if (!it->value.has_value()) {
        m_entryPointInformation->defaultLayout = { PipelineLayout { } };
        m_generatedLayout = &*m_entryPointInformation->defaultLayout;
    } else
        m_generatedLayout = nullptr;

    switch (entryPoint.stage) {
    case AST::StageAttribute::Stage::Compute:
        m_stage = ShaderStage::Compute;
        break;
    case AST::StageAttribute::Stage::Vertex:
        m_stage = ShaderStage::Vertex;
        break;
    case AST::StageAttribute::Stage::Fragment:
        m_stage = ShaderStage::Fragment;
        break;
    }

    visit(entryPoint.function);
    if (m_reads.isEmpty())
        return;

    auto usedGlobals = determineUsedGlobals();
    auto groups = m_generatedLayout
        ? insertStructs(usedGlobals.resources)
        : insertStructs(*it->value);
    insertParameters(entryPoint.function, groups);
    insertMaterializations(entryPoint.function, usedGlobals.resources);
    insertLocalDefinitions(entryPoint.function, usedGlobals.privateGlobals);
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
            auto& reference = downcast<AST::ReferenceTypeExpression>(*maybeReference);
            auto* referenceType = std::get_if<Types::Reference>(reference.inferredType());
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
        case Types::Primitive::SamplerComparison:
            return SamplerBindingLayout {
                .type = SamplerBindingType::Comparison
            };
        case Types::Primitive::TextureExternal:
            return ExternalTextureBindingLayout { };
        case Types::Primitive::AccessMode:
        case Types::Primitive::TexelFormat:
        case Types::Primitive::AddressSpace:
            RELEASE_ASSERT_NOT_REACHED();
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
        }

        return TextureBindingLayout {
            .sampleType = TextureSampleType::Float,
            .viewDimension = viewDimension,
            .multisampled = multisampled
        };
    }, [&](const TextureStorage& texture) -> BindGroupLayoutEntry::BindingMember {
        TextureViewDimension viewDimension;
        switch (texture.kind) {
        case Types::TextureStorage::Kind::TextureStorage1d:
            viewDimension = TextureViewDimension::OneDimensional;
            break;
        case Types::TextureStorage::Kind::TextureStorage2d:
            viewDimension = TextureViewDimension::TwoDimensional;
            break;
        case Types::TextureStorage::Kind::TextureStorage2dArray:
            viewDimension = TextureViewDimension::TwoDimensionalArray;
            break;
        case Types::TextureStorage::Kind::TextureStorage3d:
            viewDimension = TextureViewDimension::ThreeDimensional;
            break;
        }

        return StorageTextureBindingLayout {
            .viewDimension = viewDimension
        };
    }, [&](const TextureDepth& texture) -> BindGroupLayoutEntry::BindingMember {
        TextureViewDimension viewDimension;
        bool multisampled = false;
        switch (texture.kind) {
        case Types::TextureDepth::Kind::TextureDepth2d:
            viewDimension = TextureViewDimension::TwoDimensional;
            break;
        case Types::TextureDepth::Kind::TextureDepth2dArray:
            viewDimension = TextureViewDimension::TwoDimensionalArray;
            break;
        case Types::TextureDepth::Kind::TextureDepthCube:
            viewDimension = TextureViewDimension::Cube;
            break;
        case Types::TextureDepth::Kind::TextureDepthCubeArray:
            viewDimension = TextureViewDimension::CubeArray;
            break;
        case Types::TextureDepth::Kind::TextureDepthMultisampled2d:
            viewDimension = TextureViewDimension::TwoDimensional;
            multisampled = true;
            break;
        }

        return TextureBindingLayout {
            .sampleType = TextureSampleType::Depth,
            .viewDimension = viewDimension,
            .multisampled = multisampled
        };
    }, [&](const Atomic&) -> BindGroupLayoutEntry::BindingMember {
        return BufferBindingLayout {
            .type = addressSpace(),
            .hasDynamicOffset = false,
            .minBindingSize = 0
        };
    }, [&](const Reference&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const Pointer&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const Function&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const TypeConstructor&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    }, [&](const Bottom&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
    });
}

auto RewriteGlobalVariables::determineUsedGlobals() -> UsedGlobals
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
            continue;
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

        if (m_generatedLayout) {
            if (m_generatedLayout->bindGroupLayouts.size() <= group)
                m_generatedLayout->bindGroupLayouts.grow(group + 1);

            m_generatedLayout->bindGroupLayouts[group].entries.append({
                .binding = global.resource->binding,
                .visibility = m_stage,
                .bindingMember = bindingMemberForGlobal(global),
                .name = global.declaration->name()
            });
        }
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
    case Types::Primitive::SamplerComparison:
    case Types::Primitive::TextureExternal:
    case Types::Primitive::AccessMode:
    case Types::Primitive::TexelFormat:
    case Types::Primitive::AddressSpace:
        RELEASE_ASSERT_NOT_REACHED();
    }

    String originalName = variable.originalName();
    for (auto& attribute : variable.attributes()) {
        if (is<AST::IdAttribute>(attribute)) {
            originalName = String::number(downcast<AST::IdAttribute>(attribute).value().constantValue()->toInt());
            break;
        }
    }

    m_entryPointInformation->specializationConstants.add(originalName, Reflection::SpecializationConstant { variable.name(), constantType, variable.maybeInitializer() });
}

Vector<unsigned> RewriteGlobalVariables::insertStructs(const UsedResources& usedResources)
{
    Vector<unsigned> groups;
    for (auto& groupBinding : m_groupBindingMap) {
        unsigned group = groupBinding.key;

        auto usedResource = usedResources.find(group);
        if (usedResource == usedResources.end())
            continue;

        auto& bindingGlobalMap = groupBinding.value;
        const IndexMap<Global*>& usedBindings = usedResource->value;

        Vector<std::pair<unsigned, AST::StructureMember*>> entries;
        for (auto [binding, globalName] : bindingGlobalMap) {
            if (!usedBindings.contains(binding))
                continue;
            if (!m_reads.contains(globalName))
                continue;

            auto it = m_globals.find(globalName);
            RELEASE_ASSERT(it != m_globals.end());

            auto& global = it->value;
            ASSERT(global.declaration->maybeTypeName());

            entries.append({ binding, &createArgumentBufferEntry(binding, *global.declaration) });
        }

        if (entries.isEmpty())
            continue;

        groups.append(group);
        finalizeArgumentBufferStruct(groupBinding.key, entries);
    }
    return groups;
}

AST::StructureMember& RewriteGlobalVariables::createArgumentBufferEntry(unsigned binding, AST::Variable& variable)
{
    return createArgumentBufferEntry(binding, variable.span(), variable.name(), *variable.maybeReferenceType());
}


AST::StructureMember& RewriteGlobalVariables::createArgumentBufferEntry(unsigned binding, const SourceSpan& span, const String& name, AST::Expression& type)
{
    auto& bindingValue = m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(span, binding);
    bindingValue.m_inferredType = m_callGraph.ast().types().abstractIntType();
    bindingValue.setConstantValue(binding);
    auto& bindingAttribute = m_callGraph.ast().astBuilder().construct<AST::BindingAttribute>(span, bindingValue);
    return m_callGraph.ast().astBuilder().construct<AST::StructureMember>(
        span,
        AST::Identifier::make(name),
        type,
        AST::Attribute::List { bindingAttribute }
    );
}

void RewriteGlobalVariables::finalizeArgumentBufferStruct(unsigned group, Vector<std::pair<unsigned, AST::StructureMember*>>& entries)
{
    std::sort(entries.begin(), entries.end(), [&](auto& a, auto& b) {
        return a.first < b.first;
    });

    AST::StructureMember::List structMembers;
    for (auto& [_, member] : entries)
        structMembers.append(*member);

    m_callGraph.ast().append(m_callGraph.ast().structures(), m_callGraph.ast().astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        argumentBufferStructName(group),
        WTFMove(structMembers),
        AST::Attribute::List { },
        AST::StructureRole::BindGroup
    ));
    m_structTypes.add(group, m_callGraph.ast().types().structType(m_callGraph.ast().structures().last()));
}

Vector<unsigned> RewriteGlobalVariables::insertStructs(const PipelineLayout& layout)
{
    Vector<unsigned> groups;
    unsigned group = 0;
    for (const auto& bindGroupLayout : layout.bindGroupLayouts) {
        Vector<std::pair<unsigned, AST::StructureMember*>> entries;
        Vector<std::pair<unsigned, AST::Variable*>> bufferLengths;
        for (const auto& entry : bindGroupLayout.entries) {
            if (!entry.visibility.contains(m_stage))
                continue;

            auto argumentBufferIndex = [&] {
                switch (m_stage) {
                case ShaderStage::Vertex:
                    return entry.vertexArgumentBufferIndex;
                case ShaderStage::Fragment:
                    return entry.fragmentArgumentBufferIndex;
                case ShaderStage::Compute:
                    return entry.computeArgumentBufferIndex;
                }
            }();
            auto argumentBufferSizeIndex = [&] {
                switch (m_stage) {
                case ShaderStage::Vertex:
                    return entry.vertexArgumentBufferSizeIndex;
                case ShaderStage::Fragment:
                    return entry.fragmentArgumentBufferSizeIndex;
                case ShaderStage::Compute:
                    return entry.computeArgumentBufferSizeIndex;
                }
            }();

            AST::Variable* variable = nullptr;
            auto it = m_globalsByBinding.find({ group + 1, entry.binding + 1 });
            if (it != m_globalsByBinding.end()) {
                variable = it->value;
                entries.append({ entry.binding, &createArgumentBufferEntry(*argumentBufferIndex, *variable) });
            } else {
                auto& type = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("u32"_s));
                type.m_inferredType = m_callGraph.ast().types().u32Type();

                auto& referenceType = m_callGraph.ast().astBuilder().construct<AST::ReferenceTypeExpression>(
                    SourceSpan::empty(),
                    type
                );
                referenceType.m_inferredType = m_callGraph.ast().types().referenceType(AddressSpace::Storage, m_callGraph.ast().types().u32Type(), AccessMode::Read);
                entries.append({
                    entry.binding,
                    &createArgumentBufferEntry(*argumentBufferIndex, SourceSpan::empty(), makeString("__ArgumentBufferPlaceholder_", String::number(entry.binding)), referenceType)
                });
            }

            if (argumentBufferSizeIndex.has_value())
                bufferLengths.append({ *argumentBufferSizeIndex, variable });
        }

        for (auto [binding, variable] : bufferLengths) {
            if (variable) {
                auto it = m_bufferLengthMap.find(variable);
                RELEASE_ASSERT(it != m_bufferLengthMap.end());
                entries.append({ binding, &createArgumentBufferEntry(binding, *it->value) });
            } else {
                entries.append({
                    binding,
                    &createArgumentBufferEntry(binding, SourceSpan::empty(), makeString("__ArgumentBufferPlaceholder_", String::number(binding)), bufferLengthType())
                });
            }
        }

        if (entries.isEmpty())
            continue;

        groups.append(group);
        finalizeArgumentBufferStruct(group++, entries);
    }
    return groups;
}

void RewriteGlobalVariables::insertParameters(AST::Function& function, const Vector<unsigned>& groups)
{
    auto span = function.span();
    for (auto group : groups) {
        auto& type = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(span, argumentBufferStructName(group));
        type.m_inferredType = m_structTypes.get(group);
        auto& groupValue = m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(span, group);
        groupValue.m_inferredType = m_callGraph.ast().types().abstractIntType();
        groupValue.setConstantValue(group);
        auto& groupAttribute = m_callGraph.ast().astBuilder().construct<AST::GroupAttribute>(span, groupValue);
        m_callGraph.ast().append(function.parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
            span,
            argumentBufferParameterName(group),
            type,
            AST::Attribute::List { groupAttribute },
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
    if (variable.flavor() == AST::VariableFlavor::Const)
        return;
    if (context == Context::Global) {
        dataLogLnIf(shouldLogGlobalVariableRewriting, "> read global: ", identifier.identifier(), " at line:", identifier.span().line, " column: ", identifier.span().lineOffset);
        m_reads.add(identifier.identifier());
    }
}

void RewriteGlobalVariables::insertBeforeCurrentStatement(AST::Statement& statement)
{
    m_pendingInsertions.append({ &statement, m_currentStatementIndex });
}

AST::Identifier RewriteGlobalVariables::argumentBufferParameterName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBuffer_", String::number(group)));
}

AST::Identifier RewriteGlobalVariables::argumentBufferStructName(unsigned group)
{
    return AST::Identifier::make(makeString("__ArgumentBufferT_", String::number(group)));
}

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts).run();
}

} // namespace WGSL
