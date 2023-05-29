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
#include "ContextProviderInlines.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>

namespace WGSL {

constexpr bool shouldDumpConstantValues = false;

// A constant value might be:
// - a scalar
// - a vector
// - a matrix
// - a fixed-size array type
// - a structure
using BaseValue = std::variant<double, int64_t, bool>;
struct ConstantValue : BaseValue {
    using BaseValue::BaseValue;

    void dump(PrintStream&) const;
};

void ConstantValue::dump(PrintStream& out) const
{
    WTF::switchOn(*this,
        [&](double d) {
            out.print(String::number(d));
        },
        [&](int64_t i) {
            out.print(String::number(i));
        },
        [&](bool b) {
            out.print(b ? "true" : "false");
        });
}

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

    void materialize(AST::Expression&, ConstantValue);

    ShaderModule& m_shaderModule;
    Vector<Error> m_errors;
};

ConstantRewriter::ConstantRewriter(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
{
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
    ConstantValue result;
    switch (expression.kind()) {
    case AST::NodeKind::AbstractFloatLiteral:
        result = downcast<AST::AbstractFloatLiteral>(expression).value();
        break;
    case AST::NodeKind::AbstractIntegerLiteral:
        result = downcast<AST::AbstractIntegerLiteral>(expression).value();
        break;
    case AST::NodeKind::Float32Literal:
        result = downcast<AST::Float32Literal>(expression).value();
        break;
    case AST::NodeKind::Signed32Literal:
        result = downcast<AST::Signed32Literal>(expression).value();
        break;
    case AST::NodeKind::Unsigned32Literal:
        result = downcast<AST::Unsigned32Literal>(expression).value();
        break;
    case AST::NodeKind::BoolLiteral:
        result = downcast<AST::BoolLiteral>(expression).value();
        break;

    case AST::NodeKind::IdentifierExpression:
        result = evaluate(downcast<AST::IdentifierExpression>(expression));
        break;

    default:
        ASSERT_NOT_REACHED("Unhandled Expression");
    }

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

void ConstantRewriter::materialize(AST::Expression& expression, ConstantValue value)
{
    using namespace Types;

    const auto& replace = [&]<typename Node, typename Value>() {
        m_shaderModule.replace(expression, m_shaderModule.astBuilder().construct<Node>(SourceSpan::empty(), std::get<Value>(value)));
    };

    return WTF::switchOn(*expression.inferredType(),
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
            case Primitive::AbstractInt:
                replace.operator()<AST::AbstractIntegerLiteral, int64_t>();
                break;
            case Primitive::I32:
                replace.operator()<AST::Signed32Literal, int64_t>();
                break;
            case Primitive::U32:
                replace.operator()<AST::Unsigned32Literal, int64_t>();
                break;
            case Primitive::AbstractFloat:
                replace.operator()<AST::AbstractFloatLiteral, double>();
                break;
            case Primitive::F32:
                replace.operator()<AST::Float32Literal, double>();
                break;
            case Primitive::Bool:
                replace.operator()<AST::BoolLiteral, bool>();
                break;
            case Primitive::Void:
            case Primitive::Sampler:
            case Primitive::TextureExternal:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Vector& vector) {
            // FIXME: implement
            UNUSED_PARAM(vector);
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Matrix& matrix) {
            // FIXME: implement
            UNUSED_PARAM(matrix);
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Array& array) {
            // FIXME: implement
            UNUSED_PARAM(array);
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Struct& structure) {
            // FIXME: implement
            UNUSED_PARAM(structure);
            RELEASE_ASSERT_NOT_REACHED();
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
