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
    void visit(AST::ForStatement&) override;

    // Expressions
    void visit(AST::Expression&) override;
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
    void introduceVariable(const String&, const std::optional<ConstantValue>&);

    template<typename Node>
    void evaluated(Node&, ConstantValue);

    template<typename Node>
    void materialize(Node&, const ConstantValue&);

    using ConstantFunction = ConstantValue(*)(AST::CallExpression&, const FixedVector<ConstantValue>&);

    ShaderModule& m_shaderModule;
    Vector<Error> m_errors;
    HashMap<String, ConstantFunction> m_constantFunctions;
    std::optional<ConstantValue> m_evaluationResult;
};

ConstantRewriter::ConstantRewriter(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
{
    m_constantFunctions.add("pow"_s, constantPow);
    m_constantFunctions.add("vec2"_s, constantVector2);
    m_constantFunctions.add("vec2f"_s, constantVector2);
    m_constantFunctions.add("vec2i"_s, constantVector2);
    m_constantFunctions.add("vec2u"_s, constantVector2);
    m_constantFunctions.add("vec3i"_s, constantVector3);
    m_constantFunctions.add("vec3"_s, constantVector3);
    m_constantFunctions.add("vec3f"_s, constantVector3);
    m_constantFunctions.add("vec3u"_s, constantVector3);
    m_constantFunctions.add("vec4u"_s, constantVector4);
    m_constantFunctions.add("vec4"_s, constantVector4);
    m_constantFunctions.add("vec4f"_s, constantVector4);
    m_constantFunctions.add("vec4i"_s, constantVector4);
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

void ConstantRewriter::visit(AST::ForStatement& statement)
{
    ContextScope forScope(this);
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
void ConstantRewriter::visit(AST::Expression& expression)
{
    m_evaluationResult = std::nullopt;
    AST::Visitor::visit(expression);
}

std::optional<ConstantValue> ConstantRewriter::evaluate(AST::Expression& expression)
{
    AST::Visitor::visit(expression);
    if (shouldDumpConstantValues) {
        dataLog("> Evaluated expression: ");
        dumpNode(WTF::dataFile(), expression);
        dataLogLn(" = ", m_evaluationResult);
    }
    return std::exchange(m_evaluationResult, std::nullopt);
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
    bool isNamedType = is<AST::IdentifierExpression>(target);
    bool isParameterizedType = !isNamedType && is<AST::ElaboratedTypeExpression>(target);
    if (isNamedType || isParameterizedType) {
        AST::Identifier targetName = isParameterizedType
            ? downcast<AST::ElaboratedTypeExpression>(target).base()
            : downcast<AST::IdentifierExpression>(target).identifier();

        auto it = m_constantFunctions.find(targetName);
        if (it != m_constantFunctions.end()) {
            if (shouldDumpConstantValues) {
                dataLog("> Evaluating call: ");
                dumpNode(WTF::dataFile(), call);
                dataLog(" => ", targetName, "(");
                bool first = true;
                for (unsigned i = 0; i < argumentCount; ++i) {
                    if (!first)
                        dataLog(", ");
                    first = false;
                    dataLog(arguments[i]);
                }
                dataLogLn(")");
            }
            materialize(call, it->value(call, arguments));
            return;
        }
    }

    if (is<AST::ArrayTypeExpression>(target)) {
        evaluated(call, ConstantArray(WTFMove(arguments)));
        return;
    }
}

// Literals
void ConstantRewriter::visit(AST::BoolLiteral& literal)
{
    evaluated(literal, literal.value());
}

void ConstantRewriter::visit(AST::Signed32Literal& literal)
{
    evaluated(literal, literal.value());
}

void ConstantRewriter::visit(AST::Float32Literal& literal)
{
    evaluated(literal, literal.value());
}

void ConstantRewriter::visit(AST::Unsigned32Literal& literal)
{
    evaluated(literal, literal.value());
}

void ConstantRewriter::visit(AST::AbstractIntegerLiteral& literal)
{
    evaluated(literal, literal.value());
}

void ConstantRewriter::visit(AST::AbstractFloatLiteral& literal)
{
    evaluated(literal, literal.value());
}

template<typename Node>
void ConstantRewriter::materialize(Node& expression, const ConstantValue& value)
{
    using namespace Types;

    evaluated(expression, value);

    const auto& replace = [&]<typename Literal, typename Value>() {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=262068
        auto* maybeValue = std::get_if<Value>(&value);
        Value valueOrDefault = maybeValue ? *maybeValue : (Value)0;
        auto& node = m_shaderModule.astBuilder().construct<Literal>(SourceSpan::empty(), valueOrDefault);
        node.m_inferredType = expression.inferredType();
        m_shaderModule.replace(expression, node);
    };

    return WTF::switchOn(*expression.inferredType(),
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
            case Primitive::AbstractFloat: {
                auto& node =  m_shaderModule.astBuilder().construct<AST::AbstractFloatLiteral>(SourceSpan::empty(), value.toDouble());
                node.m_inferredType = expression.inferredType();
                m_shaderModule.replace(expression, node);
                break;
            }
            case Primitive::F32: {
                auto& node =  m_shaderModule.astBuilder().construct<AST::Float32Literal>(SourceSpan::empty(), value.toDouble());
                node.m_inferredType = expression.inferredType();
                m_shaderModule.replace(expression, node);
                break;
            }
            case Primitive::Bool:
                replace.template operator()<AST::BoolLiteral, bool>();
                break;
            case Primitive::Void:
            case Primitive::Sampler:
            case Primitive::TextureExternal:
            case Primitive::AccessMode:
            case Primitive::TexelFormat:
            case Primitive::AddressSpace:
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
        [&](const Pointer&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Function&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TextureStorage&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TextureDepth&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Atomic&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TypeConstructor&) {
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
        dataLogLn(" = ", m_evaluationResult);
    }
}

void ConstantRewriter::introduceVariable(const String& name, const std::optional<ConstantValue>& value)
{
    if (shouldDumpConstantValues)
        dataLogLn("> Assigning value: ", name, " => ", value);
    ContextProvider::introduceVariable(name, value);
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
