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
#include "StyleCalculationTree.h"

#include "StyleCalculationTree+Traversal.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {
namespace Calculation {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Abs);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Acos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Asin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Atan2);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Blend);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Clamp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Cos);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Exp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Hypot);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Invert);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Log);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Max);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Min);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Mod);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Negate);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Pow);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Product);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Progress);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Random);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Rem);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundDown);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundNearest);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundToZero);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(RoundUp);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sign);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sin);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sqrt);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Sum);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Tan);

Child number(double value)
{
    return makeChild(Number { .value = value });
}

Child percentage(double value)
{
    return makeChild(Percentage { .value = value });
}

Child dimension(double value)
{
    return makeChild(Dimension { .value = value });
}

Child add(Child&& a, Child&& b)
{
    Vector<Child> sumChildren;
    sumChildren.append(WTF::move(a));
    sumChildren.append(WTF::move(b));
    return makeChild(Sum { .children = WTF::move(sumChildren) });
}

Child multiply(Child&& a, Child&& b)
{
    Vector<Child> productChildren;
    productChildren.append(WTF::move(a));
    productChildren.append(WTF::move(b));
    return makeChild(Product { .children = WTF::move(productChildren) });
}

Child subtract(Child&& a, Child&& b)
{
    return add(WTF::move(a), makeChild(Negate { .a = WTF::move(b) }));
}

Child blend(Child&& from, Child&& to, double progress)
{
    return makeChild(Blend { .progress = progress, .from = WTF::move(from), .to = WTF::move(to) });
}

static size_t computeDepth(const Child& root)
{
    size_t maximumChildDepth = 0;
    forAllChildren(root, WTF::makeVisitor(
        [&](const std::optional<Child>& child) {
            if (child)
                maximumChildDepth = std::max(computeDepth(*child), maximumChildDepth);
        },
        [&](const Child& child) {
            maximumChildDepth = std::max(computeDepth(child), maximumChildDepth);
        },
        [&](const ChildOrNone& childOrNone) {
            if (childOrNone.holdsAlternative<Child>())
                maximumChildDepth = std::max(computeDepth(get<Child>(childOrNone)), maximumChildDepth);
        },
        [&](const auto&) {
            maximumChildDepth = std::max<size_t>(1, maximumChildDepth);
        }
    ));
    return maximumChildDepth + 1;
}

size_t computeDepth(const Tree& tree)
{
    return computeDepth(tree.root);
}

template<typename Op>
static auto dumpVariadic(TextStream&, const IndirectNode<Op>&, ASCIILiteral prefix, ASCIILiteral between) -> TextStream&;

template<typename Op>
static auto operator<<(TextStream&, const IndirectNode<Op>&) -> TextStream&;
static auto operator<<(TextStream&, const Random::Fixed&) -> TextStream&;
static auto operator<<(TextStream&, const ChildOrNone&) -> TextStream&;
static auto operator<<(TextStream&, const Child&) -> TextStream&;
static auto operator<<(TextStream&, const Number&) -> TextStream&;
static auto operator<<(TextStream&, const Percentage&) -> TextStream&;
static auto operator<<(TextStream&, const Dimension&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Sum>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Product>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Negate>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Invert>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Min>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Max>&) -> TextStream&;
static auto operator<<(TextStream&, const IndirectNode<Hypot>&) -> TextStream&;

// MARK: Dumping

template<typename Op> TextStream& dumpVariadic(TextStream& ts, const IndirectNode<Op>& root, ASCIILiteral prefix, ASCIILiteral between)
{
    ts << prefix << '(';

    auto separator = ""_s;
    for (auto& child : root->children)
        ts << std::exchange(separator, between) << child;

    return ts << ')';
}

template<typename Op> auto operator<<(TextStream& ts, const IndirectNode<Op>& root) -> TextStream&
{
    ts << Op::op << '(';

    auto separator = ""_s;
    forAllChildren(*root, WTF::makeVisitor(
        [&](const std::optional<Child>& root) {
            if (root)
                ts << std::exchange(separator, ", "_s) << *root;
        },
        [&](const auto& root) {
            ts << std::exchange(separator, ", "_s) << root;
        }
    ));

    return ts << ')';
}

TextStream& operator<<(TextStream& ts, const Random::Fixed& fixed)
{
    return ts << "fixed "_s << fixed.baseValue;
}

TextStream& operator<<(TextStream& ts, const ChildOrNone& root)
{
    return WTF::switchOn(root, [&](const auto& root) -> TextStream& { return ts << root; });
}

TextStream& operator<<(TextStream& ts, const Child& root)
{
    return WTF::switchOn(root, [&](const auto& root) -> TextStream& { return ts << root; });
}

TextStream& operator<<(TextStream& ts, const Number& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value);
}

TextStream& operator<<(TextStream& ts, const Percentage& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value) << '%';
}

TextStream& operator<<(TextStream& ts, const Dimension& root)
{
    return ts << TextStream::FormatNumberRespectingIntegers(root.value);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Sum>& root)
{
    return dumpVariadic(ts, root, ""_s, " + "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Product>& root)
{
    return dumpVariadic(ts, root, ""_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Negate>& root)
{
    return ts << "-("_s << root->a << ')';
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Invert>& root)
{
    return ts << "1.0 / ("_s << root->a << ')';
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Min>& root)
{
    return dumpVariadic(ts, root, "min"_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Max>& root)
{
    return dumpVariadic(ts, root, "max"_s, " * "_s);
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Hypot>& root)
{
    return dumpVariadic(ts, root, "hypot"_s, ", "_s);
}


TextStream& operator<<(TextStream& ts, const Tree& tree)
{
    return ts << tree.root;
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
