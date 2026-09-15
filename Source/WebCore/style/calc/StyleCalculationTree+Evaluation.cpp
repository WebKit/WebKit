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
#include "StyleCalculationTree+Evaluation.h"

#include "CSSCalcExecutor.h"
#include "StyleCalculationTree.h"
#include "StyleZoomPrimitives.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Style {
namespace Calculation {

static auto evaluate(const CSS::Keyword::None&, const EvaluationOptions&) -> CSS::Keyword::None;
static auto evaluate(const ChildOrNone&, const EvaluationOptions&) -> Variant<double, CSS::Keyword::None>;
static auto evaluate(const std::optional<Child>&, const EvaluationOptions&) -> std::optional<double>;
static auto evaluate(const Child&, const EvaluationOptions&) -> double;
static auto evaluate(const Number&, const EvaluationOptions&) -> double;
static auto evaluate(const Percentage&, const EvaluationOptions&) -> double;
static auto evaluate(const Dimension&, const EvaluationOptions&) -> double;
static auto evaluate(const Size&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Sum>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Product>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Min>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Max>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Hypot>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<Random>&, const EvaluationOptions&) -> double;
static auto evaluate(const IndirectNode<CalcMix>&, const EvaluationOptions&) -> double;
template<typename Op>
static auto evaluate(const IndirectNode<Op>&, const EvaluationOptions&) -> double;

template<typename Op, typename... Args> static double executeMathOperation(Args&&... args)
{
    return CSSCalc::executeOperation<Op::op>(std::forward<Args>(args)...);
}

// MARK: Evaluation.

CSS::Keyword::None evaluate(const CSS::Keyword::None& root, const EvaluationOptions&)
{
    return root;
}

Variant<double, CSS::Keyword::None> evaluate(const ChildOrNone& root, const EvaluationOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) {
        return Variant<double, CSS::Keyword::None> { evaluate(root, options) };
    });
}

double evaluate(const Child& root, const EvaluationOptions& options)
{
    return WTF::switchOn(root, [&](const auto& root) {
        return evaluate(root, options);
    });
}

std::optional<double> evaluate(const std::optional<Child>& root, const EvaluationOptions& options)
{
    if (root)
        return static_cast<double>(evaluate(*root, options));
    return std::nullopt;
}

double evaluate(const Number& number, const EvaluationOptions&)
{
    // A number is dimensionless, so zoom does not apply. Reachable since `size` keeps a calculation
    // from folding to a single dimension.
    return number.value;
}

double evaluate(const Percentage& percentage, const EvaluationOptions& options)
{
    return options.percentResolutionLength * percentage.value / 100.0;
}

double evaluate(const Dimension& root, const EvaluationOptions& options)
{
    return root.value * options.usedZoom.value;
}

double evaluate(const Size&, const EvaluationOptions& options)
{
    // Already a used value, so no zoom is applied, matching percentResolutionLength.
    ASSERT(options.sizeResolutionLength);
    return options.sizeResolutionLength.value_or(0);
}

double evaluate(const IndirectNode<Sum>& root, const EvaluationOptions& options)
{
    return executeMathOperation<Sum>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, options);
    });
}

double evaluate(const IndirectNode<Product>& root, const EvaluationOptions& options)
{
    return executeMathOperation<Product>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, options);
    });
}

double evaluate(const IndirectNode<Min>& root, const EvaluationOptions& options)
{
    return executeMathOperation<Min>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, options);
    });
}

double evaluate(const IndirectNode<Max>& root, const EvaluationOptions& options)
{
    return executeMathOperation<Max>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, options);
    });
}

double evaluate(const IndirectNode<Hypot>& root, const EvaluationOptions& options)
{
    return executeMathOperation<Hypot>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, options);
    });
}

double evaluate(const IndirectNode<Random>& root, const EvaluationOptions& options)
{
    auto min = evaluate(root->min, options);
    auto max = evaluate(root->max, options);
    auto step = evaluate(root->step, options);

    return executeMathOperation<Random>(root->fixed.baseValue, min, max, step);
}

double evaluate(const IndirectNode<CalcMix>& root, const EvaluationOptions& options)
{
    return executeMathOperation<CalcMix>(root->children, [&](const auto& item) -> std::pair<double, double> {
        return { evaluate(item.value, options), item.weight };
    });
}

template<typename Op> double evaluate(const IndirectNode<Op>& root, const EvaluationOptions& options)
{
    return WTF::apply([&](const auto& ...x) {
        return executeMathOperation<Op>(evaluate(x, options)...);
    } , *root);
}

double evaluate(const Tree& tree, const EvaluationOptions& options)
{
    return evaluate(tree.root, options);
}


} // namespace Calculation
} // namespace Style
} // namespace WebCore
