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
#include <wtf/ListHashSet.h>
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
    void visit(AST::VariableStatement&) override;

    void visit(AST::Expression&) override;

private:
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
    static AST::Identifier dynamicOffsetVariableName();

    void def(const AST::Identifier&, AST::Variable*);

    void collectGlobals();
    void visitEntryPoint(const CallGraph::EntryPoint&);
    void visitCallee(const CallGraph::Callee&);
    UsedGlobals determineUsedGlobals();
    void collectDynamicOffsetGlobals(const PipelineLayout&);
    void usesOverride(AST::Variable&);
    Vector<unsigned> insertStructs(const UsedResources&);
    Vector<unsigned> insertStructs(const PipelineLayout&);
    AST::StructureMember& createArgumentBufferEntry(unsigned binding, AST::Variable&);
    AST::StructureMember& createArgumentBufferEntry(unsigned binding, const SourceSpan&, const String& name, AST::Expression& type);
    void finalizeArgumentBufferStruct(unsigned group, Vector<std::pair<unsigned, AST::StructureMember*>>&);
    void insertParameters(AST::Function&, const Vector<unsigned>&);
    void insertMaterializations(AST::Function&, const UsedResources&);
    void insertLocalDefinitions(AST::Function&, const UsedPrivateGlobals&);
    void readVariable(AST::IdentifierExpression&, const Global&);
    void insertBeforeCurrentStatement(AST::Statement&);
    AST::Expression& bufferLengthType();
    AST::Expression& bufferLengthReferenceType();

    // zero initialization
    void initializeVariables(AST::Function&, const UsedPrivateGlobals&, size_t);
    void insertWorkgroupBarrier(AST::Function&, size_t);
    AST::Identifier& findOrInsertLocalInvocationIndex(AST::Function&);
    AST::Statement::List storeInitialValue(const UsedPrivateGlobals&);
    void storeInitialValue(AST::Expression&, AST::Statement::List&, unsigned, bool isNested);

    void packResource(AST::Variable&);
    void packArrayResource(AST::Variable&, const Types::Array*);
    void packStructResource(AST::Variable&, const Types::Struct*);
    const Type* packType(const Type*);
    const Type* packStructType(const Types::Struct*);
    const Type* packArrayType(const Types::Array*);
    void updateReference(AST::Variable&, AST::Expression&);

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
    ListHashSet<String> m_reads;
    HashMap<AST::Function*, ListHashSet<String>> m_visitedFunctions;
    Reflection::EntryPointInformation* m_entryPointInformation { nullptr };
    HashMap<uint32_t, uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_generateLayoutGroupMapping;
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
    HashMap<std::pair<unsigned, unsigned>, unsigned> m_globalsUsingDynamicOffset;
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
    ListHashSet<String> reads;
    for (auto& callee : m_callGraph.callees(function)) {
        visitCallee(callee);
        for (const auto& read : m_reads)
            reads.add(read);
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
    if (lhsPacking == Packing::PackedVec3)
        lhsPacking = Packing::Either;
    else
        lhsPacking = static_cast<Packing>(lhsPacking | Packing::Vec3);
    pack(lhsPacking, statement.rhs());
}

void RewriteGlobalVariables::visit(AST::VariableStatement& statement)
{
    if (auto* initializer = statement.variable().maybeInitializer())
        pack(static_cast<Packing>(Packing::Unpacked | Packing::Vec3), *initializer);
}

void RewriteGlobalVariables::visit(AST::Expression& expression)
{
    pack(Packing::Unpacked, expression);
}

Packing RewriteGlobalVariables::pack(Packing expectedPacking, AST::Expression& expression)
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
            operation = packing & Packing::Packed ? "__unpack"_s : "__pack"_s;
        else if (std::holds_alternative<Types::Array>(*type)) {
            if (packing & Packing::Packed) {
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
                operation = packing & Packing::Packed ? "int3"_s : "packed_int3"_s;
                break;
            case Types::Primitive::U32:
                operation = packing & Packing::Packed ? "uint3"_s : "packed_uint3"_s;
                break;
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                operation = packing & Packing::Packed ? "float3"_s : "packed_float3"_s;
                break;
            case Types::Primitive::F16:
                operation = packing & Packing::Packed ? "half3"_s : "packed_half3"_s;
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

Packing RewriteGlobalVariables::getPacking(AST::IdentifierExpression& identifier)
{
    auto packing = Packing::Unpacked;

    auto def = m_defs.find(identifier.identifier());
    if (def != m_defs.end())
        return packing;

    auto it = m_globals.find(identifier.identifier());
    if (it == m_globals.end())
        return packing;
    readVariable(identifier, it->value);

    if (it->value.resource.has_value())
        return packingForType(identifier.inferredType());

    return packing;
}

Packing RewriteGlobalVariables::getPacking(AST::FieldAccessExpression& expression)
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
    auto* fieldType = structType.fields.get(expression.originalFieldName());
    return packingForType(fieldType);
}

Packing RewriteGlobalVariables::getPacking(AST::IndexAccessExpression& expression)
{
    auto basePacking = pack(Packing::Either, expression.base());
    pack(Packing::Unpacked, expression.index());
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

Packing RewriteGlobalVariables::getPacking(AST::BinaryExpression& expression)
{
    pack(Packing::Unpacked, expression.leftExpression());
    pack(Packing::Unpacked, expression.rightExpression());
    return Packing::Unpacked;
}

Packing RewriteGlobalVariables::getPacking(AST::UnaryExpression& expression)
{
    pack(Packing::Unpacked, expression.expression());
    return Packing::Unpacked;
}

Packing RewriteGlobalVariables::getPacking(AST::CallExpression& call)
{
    if (is<AST::IdentifierExpression>(call.target())) {
        auto& target = downcast<AST::IdentifierExpression>(call.target());
        if (target.identifier() == "arrayLength"_s) {
            ASSERT(call.arguments().size() == 1);
            auto arrayOffset = 0;
            const auto& getBase = [&](auto&& getBase, AST::Expression& expression) -> AST::Expression& {
                if (is<AST::IdentityExpression>(expression))
                    return getBase(getBase, downcast<AST::IdentityExpression>(expression).expression());
                if (is<AST::UnaryExpression>(expression))
                    return getBase(getBase, downcast<AST::UnaryExpression>(expression).expression());
                if (is<AST::FieldAccessExpression>(expression)) {
                    auto& fieldAccess = downcast<AST::FieldAccessExpression>(expression);
                    auto& base = fieldAccess.base();
                    auto* type = base.inferredType();
                    if (auto* reference = std::get_if<Types::Reference>(type))
                        type = reference->element;
                    auto& structure = std::get<Types::Struct>(*type).structure;
                    auto& lastMember = structure.members().last();
                    RELEASE_ASSERT(lastMember.name().id() == fieldAccess.fieldName().id());
                    arrayOffset += lastMember.offset();
                    return getBase(getBase, base);
                }
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
            auto* elementType = std::get<Types::Array>(*arrayType).element;
            auto arrayStride = elementType->size();
            arrayStride = WTF::roundUpToMultipleOf(elementType->alignment(), arrayStride);

            auto& strideExpression = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
                SourceSpan::empty(),
                arrayStride
            );
            strideExpression.m_inferredType = m_callGraph.ast().types().u32Type();

            AST::Expression* lhs = &length;
            if (arrayOffset) {
                auto& arrayOffsetExpression = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
                    SourceSpan::empty(),
                    arrayOffset
                );
                arrayOffsetExpression.m_inferredType = m_callGraph.ast().types().u32Type();
                lhs = &m_callGraph.ast().astBuilder().construct<AST::BinaryExpression>(
                    SourceSpan::empty(),
                    length,
                    arrayOffsetExpression,
                    AST::BinaryOperation::Subtract
                );
                lhs->m_inferredType = m_callGraph.ast().types().u32Type();
            }

            m_callGraph.ast().setUsesDivision();
            auto& elementCount = m_callGraph.ast().astBuilder().construct<AST::BinaryExpression>(
                SourceSpan::empty(),
                *lhs,
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

Packing RewriteGlobalVariables::packingForType(const Type* type)
{
    return type->packing();
}

void RewriteGlobalVariables::collectGlobals()
{
    Vector<std::tuple<AST::Variable*, unsigned>> bufferLengths;
    // we can't use a range-based for loop here since we might create new structs
    // and insert them into the declarations vector
    auto size = m_callGraph.ast().declarations().size();
    for (unsigned i = 0; i < size; ++i) {
        auto& declaration = m_callGraph.ast().declarations()[i];
        if (!is<AST::Variable>(declaration))
            continue;
        auto& globalVar = downcast<AST::Variable>(declaration);
        std::optional<Global::Resource> resource;
        if (globalVar.group().has_value()) {
            RELEASE_ASSERT(globalVar.binding().has_value());
            resource = { *globalVar.group(), *globalVar.binding() };
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

            if (!m_generatedLayout || globalVar.maybeReferenceType()->inferredType()->containsRuntimeArray())
                bufferLengths.append({ &globalVar, resource->group });
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

    // Ensure we pack nested structs
    bool packedAnyMember = false;
    for (auto& member : structType->structure.members()) {
        if (packType(member.type().inferredType()))
            packedAnyMember = true;
    }
    if (!packedAnyMember && !structType->structure.hasSizeOrAlignmentAttributes())
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
    m_callGraph.ast().append(m_callGraph.ast().declarations(), packedStruct);
    const Type* packedStructType = m_callGraph.ast().types().structType(packedStruct);
    packedStruct.m_inferredType = packedStructType;
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

static size_t getRoundedSize(const AST::Variable& variable)
{
    auto* type = variable.storeType();
    return roundUpToMultipleOf(16, type ? type->size() : 0);
}

void RewriteGlobalVariables::visitEntryPoint(const CallGraph::EntryPoint& entryPoint)
{
    dataLogLnIf(shouldLogGlobalVariableRewriting, "> Visiting entrypoint: ", entryPoint.function.name());

    m_reads.clear();
    m_structTypes.clear();
    m_globalsUsingDynamicOffset.clear();

    m_entryPointInformation = &entryPoint.information;

    m_stage = entryPoint.stage;

    auto it = m_pipelineLayouts.find(m_entryPointInformation->originalName);
    ASSERT(it != m_pipelineLayouts.end());

    if (!it->value.has_value()) {
        m_entryPointInformation->defaultLayout = { PipelineLayout { } };
        m_generatedLayout = &*m_entryPointInformation->defaultLayout;
    } else {
        m_generatedLayout = nullptr;
        collectDynamicOffsetGlobals(*it->value);
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
    for (auto* global : usedGlobals.privateGlobals) {
        if (!global || !global->declaration)
            continue;
        auto* variable = global->declaration;
        if (variable->addressSpace() == AddressSpace::Workgroup)
            m_entryPointInformation->sizeForWorkgroupVariables += getRoundedSize(*variable);
    }
}

void RewriteGlobalVariables::collectDynamicOffsetGlobals(const PipelineLayout& pipelineLayout)
{
    unsigned group = 0;
    for (const auto& bindGroupLayout : pipelineLayout.bindGroupLayouts) {
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
            auto bufferDynamicOffset = [&] {
                switch (m_stage) {
                case ShaderStage::Vertex:
                    return entry.vertexBufferDynamicOffset;
                case ShaderStage::Fragment:
                    return entry.fragmentBufferDynamicOffset;
                case ShaderStage::Compute:
                    return entry.computeBufferDynamicOffset;
                }
            }();

            if (!bufferDynamicOffset.has_value())
                continue;

            m_globalsUsingDynamicOffset.add({ group + 1, *argumentBufferIndex + 1 }, *bufferDynamicOffset);
        }
        ++group;
    }
}

static WGSL::StorageTextureAccess convertAccess(const AccessMode accessMode)
{
    switch (accessMode) {
    case AccessMode::Read:
        return WGSL::StorageTextureAccess::ReadOnly;
    case AccessMode::ReadWrite:
        return WGSL::StorageTextureAccess::ReadWrite;
    case AccessMode::Write:
        return WGSL::StorageTextureAccess::WriteOnly;
    }
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
                return referenceType->accessMode == AccessMode::Read ? BufferBindingType::ReadOnlyStorage : BufferBindingType::Storage;
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
        case Types::Primitive::F16:
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
            .sampleType = TextureSampleType::UnfilterableFloat,
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
            .access = convertAccess(texture.access),
            .format = texture.format,
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
    }, [&](const PrimitiveStruct&) -> BindGroupLayoutEntry::BindingMember {
        RELEASE_ASSERT_NOT_REACHED();
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
    case Types::Primitive::F16:
        constantType = Reflection::SpecializationConstantType::Half;
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

    String entryName = variable.originalName();
    if (variable.id())
        entryName = String::number(*variable.id());
    m_entryPointInformation->specializationConstants.add(entryName, Reflection::SpecializationConstant { variable.name(), constantType, variable.maybeInitializer() });
}

Vector<unsigned> RewriteGlobalVariables::insertStructs(const UsedResources& usedResources)
{
    Vector<unsigned> groups;
    for (auto& groupBinding : m_groupBindingMap) {
        auto usedResource = usedResources.find(groupBinding.key);
        if (usedResource == usedResources.end())
            continue;

        auto& bindingGlobalMap = groupBinding.value;
        const IndexMap<Global*>& usedBindings = usedResource->value;

        Vector<std::pair<unsigned, AST::StructureMember*>> entries;
        unsigned metalId = 0;
        HashMap<AST::Variable*, unsigned> bufferSizeToOwnerMap;
        for (auto [binding, globalName] : bindingGlobalMap) {
            unsigned group = groupBinding.key;
            if (!usedBindings.contains(binding))
                continue;
            if (!m_reads.contains(globalName))
                continue;

            auto it = m_globals.find(globalName);
            RELEASE_ASSERT(it != m_globals.end());

            auto& global = it->value;
            ASSERT(global.declaration->maybeTypeName());

            entries.append({ metalId, &createArgumentBufferEntry(metalId, *global.declaration) });

            if (auto it = m_generateLayoutGroupMapping.find(groupBinding.key); it != m_generateLayoutGroupMapping.end())
                group = it->value;
            else {
                auto newGroup = m_generatedLayout->bindGroupLayouts.size();
                m_generateLayoutGroupMapping.add(group, newGroup);
                m_generatedLayout->bindGroupLayouts.append({ group, { } });
                group = newGroup;
            }

            BindGroupLayoutEntry entry {
                .binding = metalId,
                .webBinding = global.resource->binding,
                .visibility = m_stage,
                .bindingMember = bindingMemberForGlobal(global),
                .name = global.declaration->name()
            };

            auto bufferSizeIt = m_bufferLengthMap.find(global.declaration);
            if (bufferSizeIt != m_bufferLengthMap.end()) {
                auto* variable = bufferSizeIt->value;
                bufferSizeToOwnerMap.add(variable, m_generatedLayout->bindGroupLayouts[group].entries.size());
            } else if (auto ownerIt = bufferSizeToOwnerMap.find(global.declaration); ownerIt != bufferSizeToOwnerMap.end()) {
                // FIXME: since we only ever generate a layout for one shader stage
                // at a time, we always store the indices in the vertex slot, but
                // we should use a structs to pass information from the compiler to
                // the API (instead of reusing the same struct the API uses to pass
                // information to the compiler)
                m_generatedLayout->bindGroupLayouts[group].entries[ownerIt->value].vertexArgumentBufferSizeIndex = metalId;
            }

            auto* type = global.declaration->storeType();
            if (isPrimitive(type, Types::Primitive::TextureExternal))
                metalId += 4;
            else
                ++metalId;

            m_generatedLayout->bindGroupLayouts[group].entries.append(WTFMove(entry));
        }

        if (entries.isEmpty())
            continue;

        groups.append(groupBinding.key);
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

    auto& argumentBufferStruct = m_callGraph.ast().astBuilder().construct<AST::Structure>(
        SourceSpan::empty(),
        argumentBufferStructName(group),
        WTFMove(structMembers),
        AST::Attribute::List { },
        AST::StructureRole::BindGroup
    );
    argumentBufferStruct.m_inferredType = m_callGraph.ast().types().structType(argumentBufferStruct);
    m_callGraph.ast().append(m_callGraph.ast().declarations(), argumentBufferStruct);
    m_structTypes.add(group, argumentBufferStruct.m_inferredType);
}

static AddressSpace addressSpaceForBindingMember(const BindGroupLayoutEntry::BindingMember& bindingMember)
{
    return WTF::switchOn(bindingMember, [](const BufferBindingLayout& bufferBinding) {
        switch (bufferBinding.type) {
        case BufferBindingType::Uniform:
            return AddressSpace::Uniform;
        case BufferBindingType::Storage:
            return AddressSpace::Storage;
        case BufferBindingType::ReadOnlyStorage:
            return AddressSpace::Storage;
        }
    }, [](const SamplerBindingLayout&) {
        return AddressSpace::Handle;
    }, [](const TextureBindingLayout&) {
        return AddressSpace::Handle;
    }, [](const StorageTextureBindingLayout&) {
        return AddressSpace::Handle;
    }, [](const ExternalTextureBindingLayout&) {
        return AddressSpace::Handle;
    });
}

static AccessMode accessModeForBindingMember(const BindGroupLayoutEntry::BindingMember& bindingMember)
{
    return WTF::switchOn(bindingMember, [](const BufferBindingLayout& bufferBinding) {
        switch (bufferBinding.type) {
        case BufferBindingType::Uniform:
            return AccessMode::Read;
        case BufferBindingType::Storage:
            return AccessMode::ReadWrite;
        case BufferBindingType::ReadOnlyStorage:
            return AccessMode::Read;
        }
    }, [](const SamplerBindingLayout&) {
        return AccessMode::Read;
    }, [](const TextureBindingLayout&) {
        return AccessMode::Read;
    }, [](const StorageTextureBindingLayout&) {
        return AccessMode::Read;
    }, [](const ExternalTextureBindingLayout&) {
        return AccessMode::Read;
    });
}

enum class BindingType {
    Undefined,
    Buffer,
    Texture,
    TextureMultisampled,
    TextureStorageReadOnly,
    TextureStorageReadWrite,
    TextureStorageWriteOnly,
    Sampler,
    SamplerComparison,
    TextureExternal,
};

static BindingType bindingTypeForPrimitive(const Types::Primitive& primitive)
{
    switch (primitive.kind) {
    case Types::Primitive::AbstractInt:
    case Types::Primitive::AbstractFloat:
    case Types::Primitive::I32:
    case Types::Primitive::U32:
    case Types::Primitive::F32:
    case Types::Primitive::F16:
    case Types::Primitive::Bool:
    case Types::Primitive::Void:
        return BindingType::Buffer;
    case Types::Primitive::Sampler:
        return BindingType::Sampler;
    case Types::Primitive::SamplerComparison:
        return BindingType::SamplerComparison;

    case Types::Primitive::TextureExternal:
        return BindingType::TextureExternal;

    case Types::Primitive::AccessMode:
    case Types::Primitive::TexelFormat:
    case Types::Primitive::AddressSpace:
        return BindingType::Undefined;
    }
}

static BindingType bindingTypeForType(const Type* type)
{
    if (!type)
        return BindingType::Undefined;

    return WTF::switchOn(*type,
        [&](const Types::Primitive& primitive) {
            return bindingTypeForPrimitive(primitive);
        },
        [&](const Types::Vector&) {
            return BindingType::Buffer;
        },
        [&](const Types::Array&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::Struct&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::PrimitiveStruct&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::Matrix&) {
            return BindingType::Buffer;
        },
        [&](const Types::Reference&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::Pointer&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::Function&) -> BindingType {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Texture& texture) {
        return texture.kind == Types::Texture::Kind::TextureMultisampled2d ? BindingType::TextureMultisampled : BindingType::Texture;
        },
        [&](const Types::TextureStorage& storageTexture) {
        switch (storageTexture.access) {
        case AccessMode::Read:
            return BindingType::TextureStorageReadOnly;
        case AccessMode::ReadWrite:
            return BindingType::TextureStorageReadWrite;
        case AccessMode::Write:
            return BindingType::TextureStorageWriteOnly;
        }
        },
        [&](const Types::TextureDepth& texture) {
            return texture.kind == Types::TextureDepth::Kind::TextureDepthMultisampled2d ? BindingType::TextureMultisampled : BindingType::Texture;
        },
        [&](const Types::Atomic&) -> BindingType {
            return BindingType::Buffer;
        },
        [&](const Types::TypeConstructor&) -> BindingType {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Types::Bottom&) -> BindingType {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

static bool isBuffer(const AST::Variable& variable)
{
    return bindingTypeForType(variable.storeType()) == BindingType::Buffer;
}

static bool isSampler(const AST::Variable& variable, SamplerBindingType bindingType)
{
    switch (bindingType) {
    case SamplerBindingType::Filtering:
    case SamplerBindingType::NonFiltering:
        return bindingTypeForType(variable.storeType()) == BindingType::Sampler;
    case SamplerBindingType::Comparison:
        return bindingTypeForType(variable.storeType()) == BindingType::SamplerComparison;
    }
}

static bool isTexture(const AST::Variable& variable, bool isMultisampled)
{
    return bindingTypeForType(variable.storeType()) == (isMultisampled ? BindingType::TextureMultisampled : BindingType::Texture);
}

static bool isStorageTexture(const AST::Variable& variable, StorageTextureAccess textureAccess)
{
    switch (bindingTypeForType(variable.storeType())) {
    case BindingType::TextureStorageReadOnly:
        return textureAccess == StorageTextureAccess::ReadOnly;
    case BindingType::TextureStorageReadWrite:
        return textureAccess == StorageTextureAccess::ReadWrite;
    case BindingType::TextureStorageWriteOnly:
        return textureAccess == StorageTextureAccess::WriteOnly || textureAccess == StorageTextureAccess::ReadWrite;
    default:
        return false;
    }
}

static bool isExternalTexture(const AST::Variable& variable)
{
    return bindingTypeForType(variable.storeType()) == BindingType::TextureExternal;
}

static bool typesMatch(const AST::Variable& variable, const BindGroupLayoutEntry::BindingMember& bindingMember)
{
    return WTF::switchOn(bindingMember, [&](const BufferBindingLayout&) {
        return isBuffer(variable);
    }, [&](const SamplerBindingLayout& samplerBinding) {
        return isSampler(variable, samplerBinding.type);
    }, [&](const TextureBindingLayout& textureBinding) {
        return isTexture(variable, textureBinding.multisampled);
    }, [&](const StorageTextureBindingLayout& storageTexture) {
        return isStorageTexture(variable, storageTexture.access);
    }, [&](const ExternalTextureBindingLayout&) {
        return isExternalTexture(variable);
    });
}

static bool variableAndEntryMatch(const AST::Variable& variable, const BindGroupLayoutEntry& entry)
{
    if (!typesMatch(variable, entry.bindingMember))
        return false;

    auto variableAddressSpace = variable.addressSpace();
    auto entryAddressSpace = addressSpaceForBindingMember(entry.bindingMember);
    if (variableAddressSpace && *variableAddressSpace != entryAddressSpace)
        return false;

    auto variableAccessMode = variable.accessMode();
    auto entryAccessMode = accessModeForBindingMember(entry.bindingMember);
    if (variableAccessMode && *variableAccessMode != entryAccessMode)
        return false;

    return true;
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
                if (!variableAndEntryMatch(*variable, entry))
                    return Vector<unsigned>();
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

        if (entries.isEmpty()) {
            ++group;
            continue;
        }

        groups.append(group);
        finalizeArgumentBufferStruct(group++, entries);
    }
    return groups;
}

void RewriteGlobalVariables::insertParameters(AST::Function& function, const Vector<unsigned>& groups)
{
    auto span = function.span();
    const auto& insertParameter = [&](unsigned group, AST::Identifier&& name, AST::Expression* type = nullptr, AST::ParameterRole parameterRole = AST::ParameterRole::BindGroup) {
        if (!type) {
            type = &m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(span, argumentBufferStructName(group));
            type->m_inferredType = m_structTypes.get(group);
        }
        auto& groupValue = m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(span, group);
        groupValue.m_inferredType = m_callGraph.ast().types().abstractIntType();
        groupValue.setConstantValue(group);
        auto& groupAttribute = m_callGraph.ast().astBuilder().construct<AST::GroupAttribute>(span, groupValue);
        m_callGraph.ast().append(function.parameters(), m_callGraph.ast().astBuilder().construct<AST::Parameter>(
            span,
            WTFMove(name),
            *type,
            AST::Attribute::List { groupAttribute },
            parameterRole
        ));
    };
    for (auto group : groups)
        insertParameter(group, argumentBufferParameterName(group));
    if (!m_globalsUsingDynamicOffset.isEmpty()) {
        unsigned group;
        switch (m_stage) {
        case ShaderStage::Vertex:
            group = m_callGraph.ast().configuration().maxBuffersPlusVertexBuffersForVertexStage;
            break;
        case ShaderStage::Fragment:
            group = m_callGraph.ast().configuration().maxBuffersForFragmentStage;
            break;
        case ShaderStage::Compute:
            group = m_callGraph.ast().configuration().maxBuffersForComputeStage;
            break;
        }

        auto& type = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(span, AST::Identifier::make("u32"_s));
        type.m_inferredType = m_callGraph.ast().types().pointerType(AddressSpace::Uniform, m_callGraph.ast().types().u32Type(), AccessMode::Read);

        insertParameter(group, AST::Identifier::make(dynamicOffsetVariableName()), &type, AST::ParameterRole::UserDefined);
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

        for (auto& [binding, global] : bindings) {
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
            AST::Expression* initializer = &access;

            auto it = m_globalsUsingDynamicOffset.find({ group + 1, binding + 1 });
            if (it != m_globalsUsingDynamicOffset.end()) {
                auto offset = it->value;
                auto& target = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
                    SourceSpan::empty(),
                    AST::Identifier::make("__dynamicOffset"_s)
                );
                auto& reference = std::get<Types::Reference>(*global->declaration->maybeReferenceType()->inferredType());
                target.m_inferredType = m_callGraph.ast().types().pointerType(reference.addressSpace, storeType, reference.accessMode);

                auto& offsetExpression = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(span, offset);
                offsetExpression.m_inferredType = m_callGraph.ast().types().u32Type();
                offsetExpression.setConstantValue(offset);
                initializer = &m_callGraph.ast().astBuilder().construct<AST::CallExpression>(
                    SourceSpan::empty(),
                    target,
                    AST::Expression::List { access, offsetExpression }
                );
            }

            auto& variable = m_callGraph.ast().astBuilder().construct<AST::Variable>(
                SourceSpan::empty(),
                AST::VariableFlavor::Let,
                AST::Identifier::make(name),
                nullptr,
                global->declaration->maybeReferenceType(),
                initializer,
                AST::Attribute::List { }
            );

            auto& variableStatement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(SourceSpan::empty(), variable);
            m_callGraph.ast().insert(function.body().statements(), 0, std::reference_wrapper<AST::Statement>(variableStatement));
        }
    }
}

void RewriteGlobalVariables::insertLocalDefinitions(AST::Function& function, const UsedPrivateGlobals& usedPrivateGlobals)
{
    auto initialBodySize = function.body().statements().size();
    for (auto* global : usedPrivateGlobals) {
        auto& variableStatement = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(SourceSpan::empty(), *global->declaration);
        m_callGraph.ast().insert(function.body().statements(), 0, std::reference_wrapper<AST::Statement>(variableStatement));
    }

    auto offset = function.body().statements().size() - initialBodySize;
    initializeVariables(function, usedPrivateGlobals, offset);
}

void RewriteGlobalVariables::initializeVariables(AST::Function& function, const UsedPrivateGlobals& globals, size_t offset)
{
    auto initializations = storeInitialValue(globals);
    if (initializations.isEmpty())
        return;

    insertWorkgroupBarrier(function, offset);

    auto localInvocationIndex = findOrInsertLocalInvocationIndex(function);

    auto& testLhs = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make(localInvocationIndex.id())
    );
    testLhs.m_inferredType = m_callGraph.ast().types().u32Type();

    auto& testRhs = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(SourceSpan::empty(), 0);
    testLhs.m_inferredType = m_callGraph.ast().types().u32Type();


    auto& testExpression = m_callGraph.ast().astBuilder().construct<AST::BinaryExpression>(
        SourceSpan::empty(),
        testLhs,
        testRhs,
        AST::BinaryOperation::Equal
    );
    testExpression.m_inferredType = m_callGraph.ast().types().boolType();

    auto& body = m_callGraph.ast().astBuilder().construct<AST::CompoundStatement>(
        SourceSpan::empty(),
        WTFMove(initializations)
    );

    auto& ifStatement = m_callGraph.ast().astBuilder().construct<AST::IfStatement>(
        SourceSpan::empty(),
        testExpression,
        body,
        nullptr,
        AST::Attribute::List { }
    );
    m_callGraph.ast().insert(function.body().statements(), offset, std::reference_wrapper<AST::Statement>(ifStatement));
}

void RewriteGlobalVariables::insertWorkgroupBarrier(AST::Function& function, size_t offset)
{
    auto& callee = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("workgroupBarrier"_s));
    callee.m_inferredType = m_callGraph.ast().types().bottomType();

    auto& call = m_callGraph.ast().astBuilder().construct<AST::CallExpression>(
        SourceSpan::empty(),
        callee,
        AST::Expression::List { }
    );
    call.m_inferredType = m_callGraph.ast().types().voidType();

    auto& callStatement = m_callGraph.ast().astBuilder().construct<AST::CallStatement>(
        SourceSpan::empty(),
        call
    );
    m_callGraph.ast().insert(function.body().statements(), offset, std::reference_wrapper<AST::Statement>(callStatement));
}

AST::Identifier& RewriteGlobalVariables::findOrInsertLocalInvocationIndex(AST::Function& function)
{
    for (auto& parameter : function.parameters()) {
        if (auto builtin = parameter.builtin(); builtin.has_value() && *builtin == Builtin::LocalInvocationIndex)
            return parameter.name();
    }

    auto& type = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
        SourceSpan::empty(),
        AST::Identifier::make("u32"_s)
    );
    type.m_inferredType = m_callGraph.ast().types().u32Type();

    auto& builtinAttribute = m_callGraph.ast().astBuilder().construct<AST::BuiltinAttribute>(
        SourceSpan::empty(),
        Builtin::LocalInvocationIndex
    );

    auto& parameter = m_callGraph.ast().astBuilder().construct<AST::Parameter>(
        SourceSpan::empty(),
        AST::Identifier::make("__localInvocationIndex"_s),
        type,
        AST::Attribute::List { builtinAttribute },
        AST::ParameterRole::UserDefined
    );

    m_callGraph.ast().append(function.parameters(), parameter);

    return parameter.name();
}

AST::Statement::List RewriteGlobalVariables::storeInitialValue(const UsedPrivateGlobals& globals)
{
    AST::Statement::List statements;
    for (auto* global : globals) {
        auto& variable = *global->declaration;

        if (auto addressSpace = variable.addressSpace(); !addressSpace.has_value() || *addressSpace != AddressSpace::Workgroup)
            continue;

        auto* type = variable.storeType();
        auto& target = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
            SourceSpan::empty(),
            AST::Identifier::make(variable.name().id())
        );
        target.m_inferredType = type;
        storeInitialValue(target, statements, 0, false);
    }
    return statements;
}

void RewriteGlobalVariables::storeInitialValue(AST::Expression& target, AST::Statement::List& statements, unsigned arrayDepth, bool isNested)
{
    const auto& zeroInitialize = [&]() {
        // This piece of code generation relies on 2 implementation details from the metal serializer:
        // - The callee's name won't be used if the call is set to constructor
        // - There's a special case to handle the case where the left-hand side
        //   of the assignment doesn't have a type, so we can erase it
        auto& callee = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("__initialize"_s));
        callee.m_inferredType = target.inferredType();

        auto& call = m_callGraph.ast().astBuilder().construct<AST::CallExpression>(
            SourceSpan::empty(),
            callee,
            AST::Expression::List { }
        );
        call.m_inferredType = target.inferredType();
        call.m_isConstructor = true;

        target.m_inferredType = nullptr;

        auto& assignmentStatement = m_callGraph.ast().astBuilder().construct<AST::AssignmentStatement>(
            SourceSpan::empty(),
            target,
            call
        );
        statements.append(AST::Statement::Ref(assignmentStatement));
    };

    auto* type = target.inferredType();
    if (auto* arrayType = std::get_if<Types::Array>(type)) {
        RELEASE_ASSERT(!arrayType->isRuntimeSized());
        String indexVariableName = makeString("__i", String::number(arrayDepth));

        auto& indexVariable = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(
            SourceSpan::empty(),
            AST::Identifier::make(indexVariableName)
        );
        indexVariable.m_inferredType = m_callGraph.ast().types().u32Type();

        auto& arrayAccess = m_callGraph.ast().astBuilder().construct<AST::IndexAccessExpression>(
            SourceSpan::empty(),
            target,
            indexVariable
        );
        arrayAccess.m_inferredType = arrayType->element;

        AST::Statement::List forBodyStatements;
        storeInitialValue(arrayAccess, forBodyStatements, arrayDepth + 1, true);

        auto& zero = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
            SourceSpan::empty(),
            0
        );
        zero.m_inferredType = m_callGraph.ast().types().u32Type();

        auto& forVariable = m_callGraph.ast().astBuilder().construct<AST::Variable>(
            SourceSpan::empty(),
            AST::VariableFlavor::Var,
            AST::Identifier::make(indexVariableName),
            nullptr,
            &zero
        );

        auto& forInitializer = m_callGraph.ast().astBuilder().construct<AST::VariableStatement>(
            SourceSpan::empty(),
            forVariable
        );

        auto* arrayLength = [&]() -> AST::Expression* {
            if (auto* overrideExpression = std::get_if<AST::Expression*>(&arrayType->size))
                return *overrideExpression;

            auto& arrayLength = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
                SourceSpan::empty(),
                std::get<unsigned>(arrayType->size)
            );
            arrayLength.m_inferredType = m_callGraph.ast().types().u32Type();
            return &arrayLength;
        }();


        auto& forTest = m_callGraph.ast().astBuilder().construct<AST::BinaryExpression>(
            SourceSpan::empty(),
            indexVariable,
            *arrayLength,
            AST::BinaryOperation::LessThan
        );
        forTest.m_inferredType = m_callGraph.ast().types().boolType();

        auto& one = m_callGraph.ast().astBuilder().construct<AST::Unsigned32Literal>(
            SourceSpan::empty(),
            1
        );
        one.m_inferredType = m_callGraph.ast().types().u32Type();

        auto& forUpdate = m_callGraph.ast().astBuilder().construct<AST::CompoundAssignmentStatement>(
            SourceSpan::empty(),
            indexVariable,
            one,
            AST::BinaryOperation::Add
        );

        auto& forBody = m_callGraph.ast().astBuilder().construct<AST::CompoundStatement>(
            SourceSpan::empty(),
            WTFMove(forBodyStatements)
        );

        auto& forStatement = m_callGraph.ast().astBuilder().construct<AST::ForStatement>(
            SourceSpan::empty(),
            &forInitializer,
            &forTest,
            &forUpdate,
            forBody
        );

        statements.append(AST::Statement::Ref(forStatement));
        return;
    }

    if (auto* structType = std::get_if<Types::Struct>(type)) {
        if (type->isConstructible()) {
            zeroInitialize();
            return;
        }

        for (auto& member : structType->structure.members()) {
            auto* fieldType = member.type().inferredType();
            auto& fieldAccess = m_callGraph.ast().astBuilder().construct<AST::FieldAccessExpression>(
                SourceSpan::empty(),
                target,
                AST::Identifier::make(member.name())
            );
            fieldAccess.m_inferredType = fieldType;
            storeInitialValue(fieldAccess, statements, arrayDepth, true);
        }
        return;
    }

    if (auto* atomicType = std::get_if<Types::Atomic>(type)) {
        auto& callee = m_callGraph.ast().astBuilder().construct<AST::IdentifierExpression>(SourceSpan::empty(), AST::Identifier::make("atomicStore"_s));
        callee.m_inferredType = m_callGraph.ast().types().bottomType();

        auto& pointer = m_callGraph.ast().astBuilder().construct<AST::UnaryExpression>(
            SourceSpan::empty(),
            target,
            AST::UnaryOperation::AddressOf
        );
        pointer.m_inferredType = m_callGraph.ast().types().bottomType();

        auto& value = m_callGraph.ast().astBuilder().construct<AST::AbstractIntegerLiteral>(SourceSpan::empty(), 0);
        value.m_inferredType = m_callGraph.ast().types().abstractIntType();

        auto& call = m_callGraph.ast().astBuilder().construct<AST::CallExpression>(
            SourceSpan::empty(),
            callee,
            AST::Expression::List { pointer, value }
        );
        call.m_inferredType = m_callGraph.ast().types().voidType();

        auto& callStatement = m_callGraph.ast().astBuilder().construct<AST::CallStatement>(
            SourceSpan::empty(),
            call
        );
        statements.append(AST::Statement::Ref(callStatement));
        return;
    }

    if (!isNested)
        return;

    zeroInitialize();
}

void RewriteGlobalVariables::def(const AST::Identifier& name, AST::Variable* variable)
{
    dataLogLnIf(shouldLogGlobalVariableRewriting, "> def: ", name, " at line:", name.span().line, " column: ", name.span().lineOffset);
    m_defs.add(name, variable);
}

void RewriteGlobalVariables::readVariable(AST::IdentifierExpression& identifier, const Global& global)
{
    if (global.declaration->flavor() == AST::VariableFlavor::Const)
        return;

    dataLogLnIf(shouldLogGlobalVariableRewriting, "> read global: ", identifier.identifier(), " at line:", identifier.span().line, " column: ", identifier.span().lineOffset);
    auto addResult = m_reads.add(identifier.identifier());
    if (addResult.isNewEntry) {
        if (auto* type = global.declaration->maybeTypeName())
            visit(*type);
        if (auto* initializer = global.declaration->maybeInitializer())
            visit(*initializer);
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

AST::Identifier RewriteGlobalVariables::dynamicOffsetVariableName()
{
    return AST::Identifier::make(makeString("__DynamicOffsets"));
}

void rewriteGlobalVariables(CallGraph& callGraph, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts)
{
    RewriteGlobalVariables(callGraph, pipelineLayouts).run();
}

} // namespace WGSL
