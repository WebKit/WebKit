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
#include "CalculationTree.h"

#include "CalculationTree+Traversal.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
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

template<typename Op>
static auto dumpVariadic(TextStream&, const IndirectNode<Op>&, ASCIILiteral prefix, ASCIILiteral between) -> TextStream&;

template<typename Op>
static auto operator<<(TextStream&, const IndirectNode<Op>&) -> TextStream&;
static auto operator<<(TextStream&, const Random::CachingOptions&) -> TextStream&;
static auto operator<<(TextStream&, const None&) -> TextStream&;
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
    ts << prefix << "(";

    auto separator = ""_s;
    for (auto& child : root->children)
        ts << std::exchange(separator, between) << child;

    return ts << ")";
}

template<typename Op> auto operator<<(TextStream& ts, const IndirectNode<Op>& root) -> TextStream&
{
    ts << Op::op << "(";

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

    return ts << ")";
}

TextStream& operator<<(TextStream& ts, const Random::CachingOptions& options)
{
    return ts << "options(id(" << options.identifier << "), per-element(" << options.perElement << "))";
}

TextStream& operator<<(TextStream& ts, const None&)
{
    return ts << "none";
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
    return ts << TextStream::FormatNumberRespectingIntegers(root.value) << "%";
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
    return ts << "-(" << root->a << ")";
}

TextStream& operator<<(TextStream& ts, const IndirectNode<Invert>& root)
{
    return ts << "1.0 / (" << root->a << ")";
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
} // namespace WebCore
