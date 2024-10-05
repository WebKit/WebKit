/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcTree+Evaluation.h"

#include "AnchorPositionEvaluator.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSCalcTree.h"
#include "CalculationExecutor.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace CSSCalc {

static auto evaluate(const CSS::NoneRaw&, const EvaluationOptions&) -> std::optional<Calculation::None>;
static auto evaluate(const ChildOrNone&, const EvaluationOptions&) -> std::optional<std::variant<double, Calculation::None>>;
static auto evaluate(const std::optional<Child>&, const EvaluationOptions&) -> std::optional<std::optional<double>>;
static auto evaluate(const Child&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const Number&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const Percentage&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const CanonicalDimension&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const NonCanonicalDimension&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const Symbol&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Sum>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Product>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Min>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Max>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Hypot>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<Anchor>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const IndirectNode<AnchorSize>&, const EvaluationOptions&) -> std::optional<double>;
template<typename Op>
static auto evaluate(const IndirectNode<Op>&, const EvaluationOptions&) -> std::optional<double>;

// MARK: Evaluation.

template<typename Op, typename... Args> static std::optional<double> executeMathOperationAfterUnwrapping(Args&&... args)
{
    if ((!args.has_value() || ...))
        return std::nullopt;

    return Calculation::executeOperation<typename Op::Base>(args.value()...);
}

template<typename Op> static std::optional<double> executeVariadicMathOperationAfterUnwrapping(const IndirectNode<Op>& op, const EvaluationOptions& options)
{
    bool failure = false;
    auto result = Calculation::executeOperation<typename Op::Base>(op->children, [&](const auto& child) -> double {
        if (auto value = evaluate(child, options))
            return *value;
        failure = true;
        return std::numeric_limits<double>::quiet_NaN();
    });

    if (failure)
        return std::nullopt;

    return result;
}

std::optional<Calculation::None> evaluate(const CSS::NoneRaw&, const EvaluationOptions&)
{
    return Calculation::None { };
}

std::optional<std::variant<double, Calculation::None>> evaluate(const ChildOrNone& root, const EvaluationOptions& options)
{
    return WTF::switchOn(root,
        [&](const auto& root) -> std::optional<std::variant<double, Calculation::None>> {
            if (auto value = evaluate(root, options))
                return std::variant<double, Calculation::None> { *value };
            return std::nullopt;
        }
    );
}

std::optional<double> evaluate(const Child& root, const EvaluationOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) { return evaluate(root, options); });
}

std::optional<std::optional<double>> evaluate(const std::optional<Child>& root, const EvaluationOptions& options)
{
    if (root)
        return std::optional<double> { evaluate(*root, options) };
    return std::optional<double> { std::nullopt };
}

std::optional<double> evaluate(const Number& number, const EvaluationOptions&)
{
    return number.value;
}

std::optional<double> evaluate(const Percentage& percentage, const EvaluationOptions&)
{
    return percentage.value;
}

std::optional<double> evaluate(const CanonicalDimension& root, const EvaluationOptions&)
{
    return root.value;
}

std::optional<double> evaluate(const NonCanonicalDimension& root, const EvaluationOptions& options)
{
    if (auto canonical = canonicalize(root, options.conversionData))
        return evaluate(*canonical, options);

    // FIXME: This is only needed while CSSToLengthConversionData is optional. Once all callers pass one in, this will go away.
    if (options.allowUnresolvedUnits)
        return root.value;

    return std::nullopt;
}

std::optional<double> evaluate(const Symbol& root, const EvaluationOptions& options)
{
    if (auto value = options.symbolTable.get(root.id))
        return evaluate(makeNumeric(value->value, root.unit), options);

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

std::optional<double> evaluate(const IndirectNode<Sum>& root, const EvaluationOptions& options)
{
    return executeVariadicMathOperationAfterUnwrapping(root, options);
}

std::optional<double> evaluate(const IndirectNode<Product>& root, const EvaluationOptions& options)
{
    return executeVariadicMathOperationAfterUnwrapping(root, options);
}

std::optional<double> evaluate(const IndirectNode<Min>& root, const EvaluationOptions& options)
{
    return executeVariadicMathOperationAfterUnwrapping(root, options);
}

std::optional<double> evaluate(const IndirectNode<Max>& root, const EvaluationOptions& options)
{
    return executeVariadicMathOperationAfterUnwrapping(root, options);
}

std::optional<double> evaluate(const IndirectNode<Hypot>& root, const EvaluationOptions& options)
{
    return executeVariadicMathOperationAfterUnwrapping(root, options);
}

std::optional<double> evaluate(const IndirectNode<Anchor>& anchor, const EvaluationOptions& options)
{
    if (!options.conversionData || !options.conversionData->styleBuilderState())
        return { };

    auto result = evaluateWithoutFallback(*anchor, options);

    // https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
    // "If any of these conditions are false, the anchor() function resolves to its specified fallback value.
    // If no fallback value is specified, it makes the declaration referencing it invalid at computed-value time."
    if (!result && anchor->fallback)
        result = evaluate(*anchor->fallback, options);

    if (!result)
        options.conversionData->styleBuilderState()->setCurrentPropertyInvalidAtComputedValueTime();

    return result;

}

std::optional<double> evaluate(const IndirectNode<AnchorSize>&, const EvaluationOptions&)
{
    // FIXME (webkit.org/b/280789): evaluate anchor-size()
    return 0.0;
}

template<typename Op> std::optional<double> evaluate(const IndirectNode<Op>& root, const EvaluationOptions& options)
{
    return WTF::apply([&](const auto& ...x) { return executeMathOperationAfterUnwrapping<Op>(evaluate(x, options)...); } , *root);
}

std::optional<double> evaluateDouble(const Tree& tree, const EvaluationOptions& options)
{
    return evaluate(tree.root, options);
}

std::optional<double> evaluateWithoutFallback(const Anchor& anchor, const EvaluationOptions& options)
{
    auto& builderState = *options.conversionData->styleBuilderState();

    auto side = WTF::switchOn(anchor.side,
        [&](const Child& percentage) -> Style::AnchorPositionEvaluator::Side {
            return evaluate(percentage, options).value_or(0) / 100;
        }, [&](CSSValueID sideID) -> Style::AnchorPositionEvaluator::Side {
            return sideID;
        }
    );

    return Style::AnchorPositionEvaluator::evaluate(builderState, anchor.elementName, side);
}

} // namespace CSSCalc
} // namespace WebCore
