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

#pragma once

#include "StyleCalculationTree.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace Style {
namespace Calculation {

// MARK: - forAllChildren

// `forAllChildren` will call the provided `functor` on all direct children of the provided node. This will include values of type `Child`, `ChildOrNone` and `std::optional<Child>`.

template<typename F, Leaf Op> void forAllChildren(const auto&, const F&)
{
    // No children.
}

template<typename F, typename Op> void forAllChildren(const Op& root, const F& functor)
{
    struct Caller {
        const F& functor;

        void operator()(const Children& children)
        {
            for (auto& child : children)
                functor(child);
        }
        void operator()(const std::optional<Child>& root)
        {
            functor(root);
        }
        void operator()(const ChildOrNone& root)
        {
            functor(root);
        }
        void operator()(const Child& root)
        {
            functor(root);
        }
        void operator()(const double& root)
        {
            functor(root);
        }
        void operator()(const Random::Fixed& root)
        {
            functor(root);
        }
    };
    auto caller = Caller { functor };
    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, root);
}

template<typename F> void forAllChildren(const Child& root, const F& functor)
{
    WTF::switchOn(root,
        [&](const Leaf auto&) { },
        [&](const auto& root) { forAllChildren(*root, functor); }
    );
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
