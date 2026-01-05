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

static auto evaluate(const CSS::Keyword::None&, double percentResolutionLength, const ZoomFactor&) -> CSS::Keyword::None;
static auto evaluate(const ChildOrNone&, double percentResolutionLength, const ZoomFactor&) -> Variant<double, CSS::Keyword::None>;
static auto evaluate(const std::optional<Child>&, double percentResolutionLength, const ZoomFactor&) -> std::optional<double>;
static auto evaluate(const Child&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const Number&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const Percentage&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const Dimension&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Sum>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Product>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Min>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Max>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Hypot>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Random>&, double percentResolutionLength, const ZoomFactor&) -> double;
static auto evaluate(const IndirectNode<Blend>&, double percentResolutionLength, const ZoomFactor&) -> double;
template<typename Op>
static auto evaluate(const IndirectNode<Op>&, double percentResolutionLength, const ZoomFactor&) -> double;

template<typename Op, typename... Args> static double executeMathOperation(Args&&... args)
{
    return CSSCalc::executeOperation<Op::op>(std::forward<Args>(args)...);
}

// MARK: Evaluation.

CSS::Keyword::None evaluate(const CSS::Keyword::None& root, double, const ZoomFactor&)
{
    return root;
}

Variant<double, CSS::Keyword::None> evaluate(const ChildOrNone& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return WTF::switchOn(root, [&](const auto& root) {
        return Variant<double, CSS::Keyword::None> { evaluate(root, percentResolutionLength, usedZoom) };
    });
}

double evaluate(const Child& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return WTF::switchOn(root, [&](const auto& root) {
        return evaluate(root, percentResolutionLength, usedZoom);
    });
}

std::optional<double> evaluate(const std::optional<Child>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    if (root)
        return static_cast<double>(evaluate(*root, percentResolutionLength, usedZoom));
    return std::nullopt;
}

double evaluate(const Number& number, double, const ZoomFactor& usedZoom)
{
    return number.value * usedZoom.value;
}

double evaluate(const Percentage& percentage, double percentResolutionLength, const ZoomFactor&)
{
    return percentResolutionLength * percentage.value / 100.0;
}

double evaluate(const Dimension& root, double, const ZoomFactor& usedZoom)
{
    return root.value * usedZoom.value;
}

double evaluate(const IndirectNode<Sum>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return executeMathOperation<Sum>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, percentResolutionLength, usedZoom);
    });
}

double evaluate(const IndirectNode<Product>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return executeMathOperation<Product>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, percentResolutionLength, usedZoom);
    });
}

double evaluate(const IndirectNode<Min>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return executeMathOperation<Min>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, percentResolutionLength, usedZoom);
    });
}

double evaluate(const IndirectNode<Max>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return executeMathOperation<Max>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, percentResolutionLength, usedZoom);
    });
}

double evaluate(const IndirectNode<Hypot>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return executeMathOperation<Hypot>(root->children.value, [&](const auto& child) -> double {
        return evaluate(child, percentResolutionLength, usedZoom);
    });
}

double evaluate(const IndirectNode<Random>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    auto min = evaluate(root->min, percentResolutionLength, usedZoom);
    auto max = evaluate(root->max, percentResolutionLength, usedZoom);
    auto step = evaluate(root->step, percentResolutionLength, usedZoom);

    return executeMathOperation<Random>(root->fixed.baseValue, min, max, step);
}

double evaluate(const IndirectNode<Blend>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return (1.0 - root->progress) * evaluate(root->from, percentResolutionLength, usedZoom) + root->progress * evaluate(root->to, percentResolutionLength, usedZoom);
}

template<typename Op> double evaluate(const IndirectNode<Op>& root, double percentResolutionLength, const ZoomFactor& usedZoom)
{
    return WTF::apply([&](const auto& ...x) {
        return executeMathOperation<Op>(evaluate(x, percentResolutionLength, usedZoom)...);
    } , *root);
}

double evaluate(const Tree& tree, double percentResolutionLength, const ZoomFactor& zoomFactor)
{
    return evaluate(tree.root, percentResolutionLength, zoomFactor);
}


double evaluate(const Tree& tree, double percentResolutionLength, const ZoomNeeded&)
{
    return evaluate(tree.root, percentResolutionLength, Style::ZoomFactor { 1.0f });
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
