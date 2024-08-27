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
#include "CalculationTree+Copy.h"

#include "CalculationTree.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Calculation {

static auto copy(double) -> double;
static auto copy(const std::optional<Child>& root) -> std::optional<Child>;
static auto copy(const None&) -> None;
static auto copy(const ChildOrNone&) -> ChildOrNone;
static auto copy(const Children&) -> Children;
static auto copy(const Child&) -> Child;
template<Leaf Op>
Child copy(const Op&);
template<typename Op>
static auto copy(const IndirectNode<Op>&) -> Child;

// MARK: Copying

Tree copy(const Tree& tree)
{
    return Tree { .root = copy(tree.root), .category = tree.category, .range = tree.range };
}

double copy(double value)
{
    return value;
}

std::optional<Child> copy(const std::optional<Child>& root)
{
    if (root)
        return copy(*root);
    return std::nullopt;
}

None copy(const None& none)
{
    return none;
}

ChildOrNone copy(const ChildOrNone& root)
{
    return WTF::switchOn(root, [&](const auto& root) { return ChildOrNone { copy(root) }; });
}

Children copy(const Children& children)
{
    return WTF::map(children, [&](const auto& child) { return copy(child); });
}

Child copy(const Child& root)
{
    return WTF::switchOn(root, [&](const auto& root) { return copy(root); });
}

template<Leaf Op> Child copy(const Op& root)
{
    return { root };
}

template<typename Op> Child copy(const IndirectNode<Op>& root)
{
    return makeChild(WTF::apply([](const auto& ...x) { return Op { copy(x)... }; } , *root));
}

} // namespace Calculation
} // namespace WebCore
