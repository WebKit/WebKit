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
#include "ConstantRewriter.h"

#include "AST.h"
#include "ASTStringDumper.h"
#include "ASTVisitor.h"
#include "ConstantFunctions.h"
#include "ContextProviderInlines.h"
#include "Overload.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/FixedVector.h>

namespace WGSL {

constexpr bool shouldDumpConstantValues = false;

class ConstantRewriter : public AST::Visitor, public ContextProvider<std::optional<ConstantValue>> {
public:
    ConstantRewriter(ShaderModule&);

    std::optional<FailedCheck> rewrite();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;

    // Statements
    void visit(AST::CompoundStatement&) override;

    // Expressions
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::CallExpression&) override;

    // Literals
    void visit(AST::AbstractFloatLiteral&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::BoolLiteral&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;

private:
    template<typename... Arguments>
    void error(const SourceSpan&, Arguments&&...);

    std::optional<ConstantValue> evaluate(AST::Expression&);

    template<typename Node>
    void evaluated(Node&, ConstantValue);

    template<typename Node>
    void materialize(Node&, const ConstantValue&);

    using ConstantFunction = ConstantValue(*)(const FixedVector<ConstantValue>&);

    ShaderModule& m_shaderModule;
    Vector<Error> m_errors;
    HashMap<String, ConstantFunction> m_constantFunctions;
    std::optional<ConstantValue> m_evaluationResult;
};

ConstantRewriter::ConstantRewriter(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
{
    m_constantFunctions.add("pow"_s, constantPow);
}

std::optional<FailedCheck> ConstantRewriter::rewrite()
{
    for (auto& structure : m_shaderModule.structures())
        visit(structure);

    for (auto& variable : m_shaderModule.variables())
        visit(variable);

    for (auto& function : m_shaderModule.functions())
        visit(function);

    if (m_errors.isEmpty())
        return std::nullopt;

    Vector<Warning> warnings { };
    return FailedCheck { WTFMove(m_errors), WTFMove(warnings) };
}

// Declarations
void ConstantRewriter::visit(AST::Structure& structure)
{
    // FIXME: implement
    UNUSED_PARAM(structure);
}

void ConstantRewriter::visit(AST::Variable& variable)
{
    if (variable.flavor() != AST::VariableFlavor::Const) {
        introduceVariable(variable.name(), std::nullopt);
        AST::Visitor::visit(variable);
        return;
    }

    ASSERT(variable.maybeInitializer());
    auto value = evaluate(*variable.maybeInitializer());
    ASSERT(value.has_value());
    introduceVariable(variable.name(), *value);
}

void ConstantRewriter::visit(AST::Function& function)
{
    ContextScope functionScope(this);
    for (auto& parameter : function.parameters())
        introduceVariable(parameter.name(), std::nullopt);

    AST::Visitor::visit(function);
}

// Statements
void ConstantRewriter::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

// Expressions
void ConstantRewriter::visit(AST::IdentifierExpression& identifier)
{
    auto* constant = readVariable(identifier.identifier());
    if (!constant || !constant->has_value())
        return;

    if (shouldDumpConstantValues)
        dataLogLn("> Replacing identifier '", identifier.identifier(), "' with constant: ", *constant);

    materialize(identifier, constant->value());
}

// Private helpers
std::optional<ConstantValue> ConstantRewriter::evaluate(AST::Expression& expression)
{
    AST::Visitor::visit(expression);

    return m_evaluationResult;
}

void ConstantRewriter::visit(AST::CallExpression& call)
{
    unsigned argumentCount = call.arguments().size();
    FixedVector<ConstantValue> arguments(argumentCount);
    for (unsigned i = 0; i < argumentCount; ++i) {
        auto value = evaluate(call.arguments()[i]);
        if (!value.has_value())
            return;
        arguments[i] = *value;
    }

    auto& target = call.target();
    bool isNamedType = is<AST::NamedTypeName>(target);
    bool isParameterizedType = !isNamedType && is<AST::ParameterizedTypeName>(target);
    if (isNamedType || isParameterizedType) {
        std::optional<AST::ParameterizedTypeName::Base> base;
        if (isParameterizedType)
            base = downcast<AST::ParameterizedTypeName>(target).base();
        else {
            auto& targetName = downcast<AST::NamedTypeName>(target).name().id();
            base = AST::ParameterizedTypeName::stringViewToKind(targetName);
        }

        if (base) {
            switch (*base) {
            case AST::ParameterizedTypeName::Base::Vec2:
            case AST::ParameterizedTypeName::Base::Vec3:
            case AST::ParameterizedTypeName::Base::Vec4: {
                evaluated(call, { call.inferredType(), ConstantVector(WTFMove(arguments)) });
                return;
            }

            case AST::ParameterizedTypeName::Base::Mat2x2:
            case AST::ParameterizedTypeName::Base::Mat2x3:
            case AST::ParameterizedTypeName::Base::Mat2x4:
            case AST::ParameterizedTypeName::Base::Mat3x2:
            case AST::ParameterizedTypeName::Base::Mat3x3:
            case AST::ParameterizedTypeName::Base::Mat3x4:
            case AST::ParameterizedTypeName::Base::Mat4x2:
            case AST::ParameterizedTypeName::Base::Mat4x3:
            case AST::ParameterizedTypeName::Base::Mat4x4:
            case AST::ParameterizedTypeName::Base::Texture1d:
            case AST::ParameterizedTypeName::Base::Texture2d:
            case AST::ParameterizedTypeName::Base::Texture2dArray:
            case AST::ParameterizedTypeName::Base::Texture3d:
            case AST::ParameterizedTypeName::Base::TextureCube:
            case AST::ParameterizedTypeName::Base::TextureCubeArray:
            case AST::ParameterizedTypeName::Base::TextureMultisampled2d:
            case AST::ParameterizedTypeName::Base::TextureStorage1d:
            case AST::ParameterizedTypeName::Base::TextureStorage2d:
            case AST::ParameterizedTypeName::Base::TextureStorage2dArray:
            case AST::ParameterizedTypeName::Base::TextureStorage3d:
                return;
            }
        }

        ASSERT(isNamedType);
        auto it = m_constantFunctions.find(downcast<AST::NamedTypeName>(target).name().id());
        if (it != m_constantFunctions.end()) {
            materialize(call, it->value(arguments));
            return;
        }
    }

    if (is<AST::ArrayTypeName>(target)) {
        evaluated(call, { call.inferredType(), ConstantArray(WTFMove(arguments)) });
        return;
    }
}

// Literals
void ConstantRewriter::visit(AST::BoolLiteral& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

void ConstantRewriter::visit(AST::Signed32Literal& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

void ConstantRewriter::visit(AST::Float32Literal& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

void ConstantRewriter::visit(AST::Unsigned32Literal& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

void ConstantRewriter::visit(AST::AbstractIntegerLiteral& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

void ConstantRewriter::visit(AST::AbstractFloatLiteral& literal)
{
    evaluated(literal, { literal.inferredType(), literal.value() });
}

template<typename Node>
void ConstantRewriter::materialize(Node& expression, const ConstantValue& value)
{
    using namespace Types;

    evaluated(expression, value);

    const auto& replace = [&]<typename Literal, typename Value>() {
        auto& node =  m_shaderModule.astBuilder().construct<Literal>(SourceSpan::empty(), std::get<Value>(value));
        node.m_inferredType = expression.inferredType();
        m_shaderModule.replace(expression, node);
    };

    return WTF::switchOn(*value.type,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
            case Primitive::AbstractInt:
                replace.template operator()<AST::AbstractIntegerLiteral, int64_t>();
                break;
            case Primitive::I32:
                replace.template operator()<AST::Signed32Literal, int64_t>();
                break;
            case Primitive::U32:
                replace.template operator()<AST::Unsigned32Literal, int64_t>();
                break;
            case Primitive::AbstractFloat:
                replace.template operator()<AST::AbstractFloatLiteral, double>();
                break;
            case Primitive::F32:
                replace.template operator()<AST::Float32Literal, double>();
                break;
            case Primitive::Bool:
                replace.template operator()<AST::BoolLiteral, bool>();
                break;
            case Primitive::Void:
            case Primitive::Sampler:
            case Primitive::TextureExternal:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Vector&) {
            // Do not materialize complex types
        },
        [&](const Matrix&) {
            // Do not materialize complex types
        },
        [&](const Array&) {
            // Do not materialize complex types
        },
        [&](const Struct&) {
            // Do not materialize complex types
        },
        [&](const Reference&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Function&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

template<typename Node>
void ConstantRewriter::evaluated(Node& expression, ConstantValue value)
{
    m_evaluationResult = value;
    if (shouldDumpConstantValues) {
        dataLog("> Evaluated expression: ");
        dumpNode(WTF::dataFile(), expression);
        dataLog(" = ");
        dataLogLn(value);
    }
}

template<typename... Arguments>
void ConstantRewriter::error(const SourceSpan& span, Arguments&&... arguments)
{
    m_errors.append({ makeString(std::forward<Arguments>(arguments)...), span });
}

std::optional<FailedCheck> rewriteConstants(ShaderModule& shaderModule)
{
    return ConstantRewriter(shaderModule).rewrite();
}

} // namespace WGSL
