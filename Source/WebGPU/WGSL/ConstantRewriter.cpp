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

class ConstantRewriter : public AST::Visitor, public ContextProvider<ConstantValue> {
public:
    ConstantRewriter(ShaderModule&);

    std::optional<FailedCheck> rewrite();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;

    // Statements
    void visit(AST::CompoundStatement&) override;

    // Expressions
    void visit(AST::IdentifierExpression&) override;

private:
    template<typename... Arguments>
    void error(const SourceSpan&, Arguments&&...);

    ConstantValue evaluate(AST::Expression&);
    ConstantValue evaluate(AST::IdentifierExpression&);
    ConstantValue evaluate(AST::CallExpression&);

    void materialize(AST::Expression&, const ConstantValue&);
    AST::Expression& materialize(const ConstantValue&);

    using ConstantFunction = ConstantValue(*)(const FixedVector<ConstantValue>&);

    ShaderModule& m_shaderModule;
    Vector<Error> m_errors;
    HashMap<String, ConstantFunction> m_constantFunctions;
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
        AST::Visitor::visit(function);

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
        if (auto* initializer = variable.maybeInitializer())
            AST::Visitor::visit(*initializer);
        return;
    }

    ASSERT(variable.maybeInitializer());
    introduceVariable(variable.name(), evaluate(*variable.maybeInitializer()));
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
    if (!constant)
        return;

    if (shouldDumpConstantValues)
        dataLogLn("> Replacing identifier '", identifier.identifier(), "' with constant: ", *constant);

    materialize(identifier, *constant);
}

// Private helpers
ConstantValue ConstantRewriter::evaluate(AST::Expression& expression)
{
    ConstantValue result = [&] {
        switch (expression.kind()) {
        case AST::NodeKind::AbstractFloatLiteral:
            return ConstantValue(expression.inferredType(), downcast<AST::AbstractFloatLiteral>(expression).value());
        case AST::NodeKind::AbstractIntegerLiteral: {
            auto value = downcast<AST::AbstractIntegerLiteral>(expression).value();
            if (!satisfies(expression.inferredType(), Constraints::ConcreteInteger)) {
                // The abstract integer was promoted to a float
                return ConstantValue(expression.inferredType(), static_cast<double>(value));
            }
            return ConstantValue(expression.inferredType(), value);
        }
        case AST::NodeKind::Float32Literal:
            return ConstantValue(expression.inferredType(), downcast<AST::Float32Literal>(expression).value());
        case AST::NodeKind::Signed32Literal:
            return ConstantValue(expression.inferredType(), downcast<AST::Signed32Literal>(expression).value());
        case AST::NodeKind::Unsigned32Literal:
            return ConstantValue(expression.inferredType(), downcast<AST::Unsigned32Literal>(expression).value());
        case AST::NodeKind::BoolLiteral:
            return ConstantValue(expression.inferredType(), downcast<AST::BoolLiteral>(expression).value());

        case AST::NodeKind::IdentifierExpression:
            return evaluate(downcast<AST::IdentifierExpression>(expression));

        case AST::NodeKind::CallExpression:
            return evaluate(downcast<AST::CallExpression>(expression));

        case AST::NodeKind::UnaryExpression:
            return evaluate(downcast<AST::UnaryExpression>(expression));

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }();

    if (shouldDumpConstantValues) {
        dataLog("> Evaluated expression: ");
        dumpNode(WTF::dataFile(), expression);
        dataLog(" = ");
        dataLogLn(result);
    }

    return result;
}

ConstantValue ConstantRewriter::evaluate(AST::IdentifierExpression& identifier)
{
    auto* value = readVariable(identifier.identifier());
    ASSERT(value);
    return *value;
}

ConstantValue ConstantRewriter::evaluate(AST::CallExpression& call)
{
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
                ConstantVector vector(call.arguments().size());
                unsigned index = 0;
                for (auto& argument : call.arguments())
                    vector.elements[index++] = evaluate(argument);
                return { call.inferredType(), vector };
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
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        ASSERT(isNamedType);
        auto it = m_constantFunctions.find(downcast<AST::NamedTypeName>(target).name().id());
        if (it != m_constantFunctions.end()) {
            FixedVector<ConstantValue> arguments(call.arguments().size());
            unsigned index = 0;
            for (auto& argument : call.arguments())
                arguments[index++] = evaluate(argument);
            return it->value(arguments);
        }
    }

    if (is<AST::ArrayTypeName>(target)) {
        ConstantArray array(call.arguments().size());
        unsigned index = 0;
        for (auto& argument : call.arguments())
            array.elements[index++] = evaluate(argument);
        return { call.inferredType(), array };
    }

    RELEASE_ASSERT_NOT_REACHED();
}


void ConstantRewriter::materialize(AST::Expression& expression, const ConstantValue& value)
{
    using namespace Types;

    const auto& replace = [&]<typename Node>() {
        m_shaderModule.replace(expression, downcast<Node>(materialize(value)));
    };

    return WTF::switchOn(*value.type,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
            case Primitive::AbstractInt:
                replace.operator()<AST::AbstractIntegerLiteral>();
                break;
            case Primitive::I32:
                replace.operator()<AST::Signed32Literal>();
                break;
            case Primitive::U32:
                replace.operator()<AST::Unsigned32Literal>();
                break;
            case Primitive::AbstractFloat:
                replace.operator()<AST::AbstractFloatLiteral>();
                break;
            case Primitive::F32:
                replace.operator()<AST::Float32Literal>();
                break;
            case Primitive::Bool:
                replace.operator()<AST::BoolLiteral>();
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

AST::Expression& ConstantRewriter::materialize(const ConstantValue& value)
{
    using namespace Types;

    const auto& constructLiteral = [&]<typename Node, typename Value>() -> AST::Expression& {
        return m_shaderModule.astBuilder().construct<Node>(SourceSpan::empty(), std::get<Value>(value));
    };

    return WTF::switchOn(*value.type,
        [&](const Primitive& primitive) -> AST::Expression& {
            switch (primitive.kind) {
            case Primitive::AbstractInt:
                return constructLiteral.operator()<AST::AbstractIntegerLiteral, int64_t>();
            case Primitive::I32:
                return constructLiteral.operator()<AST::Signed32Literal, int64_t>();
            case Primitive::U32:
                return constructLiteral.operator()<AST::Unsigned32Literal, int64_t>();
            case Primitive::AbstractFloat:
                return constructLiteral.operator()<AST::AbstractFloatLiteral, double>();
            case Primitive::F32:
                return constructLiteral.operator()<AST::Float32Literal, double>();
            case Primitive::Bool:
                return constructLiteral.operator()<AST::BoolLiteral, bool>();
            case Primitive::Void:
            case Primitive::Sampler:
            case Primitive::TextureExternal:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Vector&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Matrix&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Array&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Struct&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Reference&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Function&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) -> AST::Expression& {
            RELEASE_ASSERT_NOT_REACHED();
        });
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
