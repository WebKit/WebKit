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
#include "CalculationTree+Evaluation.h"

#include "CalculationExecutor.h"
#include "CalculationTree.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Calculation {

static auto evaluate(const None&, NumericValue percentResolutionLength) -> None;
static auto evaluate(const ChildOrNone&, NumericValue percentResolutionLength) -> std::variant<NumericValue, None>;
static auto evaluate(const std::optional<Child>&, NumericValue percentResolutionLength) -> std::optional<double>;
static auto evaluate(const Child&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const Number&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const Percent&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const Dimension&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Sum>&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Product>&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Min>&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Max>&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Hypot>&, NumericValue percentResolutionLength) -> NumericValue;
static auto evaluate(const IndirectNode<Blend>&, NumericValue percentResolutionLength) -> NumericValue;
template<typename Op>
static auto evaluate(const IndirectNode<Op>&, NumericValue percentResolutionLength) -> NumericValue;

// MARK: Evaluation.

None evaluate(const None& root, NumericValue)
{
    return root;
}

std::variant<NumericValue, None> evaluate(const ChildOrNone& root, NumericValue percentResolutionLength)
{
    return WTF::switchOn(root, [&](const auto& root) { return std::variant<NumericValue, None> { evaluate(root, percentResolutionLength) }; });
}

NumericValue evaluate(const Child& root, NumericValue percentResolutionLength)
{
    return WTF::switchOn(root, [&](const auto& root) { return evaluate(root, percentResolutionLength); });
}

std::optional<double> evaluate(const std::optional<Child>& root, NumericValue percentResolutionLength)
{
    if (root)
        return static_cast<double>(evaluate(*root, percentResolutionLength));
    return std::nullopt;
}

NumericValue evaluate(const Number& number, NumericValue)
{
    return number.value;
}

NumericValue evaluate(const Percent& percent, NumericValue percentResolutionLength)
{
    return percentResolutionLength * percent.value / 100.0f;
}

NumericValue evaluate(const Dimension& root, NumericValue)
{
    return root.value;
}

NumericValue evaluate(const IndirectNode<Sum>& root, NumericValue percentResolutionLength)
{
    return executeOperation<Sum>(root->children, [&](const auto& child) -> NumericValue {
        return evaluate(child, percentResolutionLength);
    });
}

NumericValue evaluate(const IndirectNode<Product>& root, NumericValue percentResolutionLength)
{
    return executeOperation<Product>(root->children, [&](const auto& child) -> NumericValue {
        return evaluate(child, percentResolutionLength);
    });
}

NumericValue evaluate(const IndirectNode<Min>& root, NumericValue percentResolutionLength)
{
    return executeOperation<Min>(root->children, [&](const auto& child) -> NumericValue {
        return evaluate(child, percentResolutionLength);
    });
}

NumericValue evaluate(const IndirectNode<Max>& root, NumericValue percentResolutionLength)
{
    return executeOperation<Max>(root->children, [&](const auto& child) -> NumericValue {
        return evaluate(child, percentResolutionLength);
    });
}

NumericValue evaluate(const IndirectNode<Hypot>& root, NumericValue percentResolutionLength)
{
    return executeOperation<Hypot>(root->children, [&](const auto& child) -> NumericValue {
        return evaluate(child, percentResolutionLength);
    });
}

NumericValue evaluate(const IndirectNode<Blend>& root, NumericValue percentResolutionLength)
{
    return (1.0 - root->progress) * evaluate(root->from, percentResolutionLength) + root->progress * evaluate(root->to, percentResolutionLength);
}

template<typename Op> NumericValue evaluate(const IndirectNode<Op>& root, NumericValue percentResolutionLength)
{
    return WTF::apply([&](const auto& ...x) { return executeOperation<Op>(evaluate(x, percentResolutionLength)...); } , *root);
}

NumericValue evaluate(const Tree& tree, NumericValue percentResolutionLength)
{
    return evaluate(tree.root, percentResolutionLength);
}

} // namespace Calculation
} // namespace WebCore
