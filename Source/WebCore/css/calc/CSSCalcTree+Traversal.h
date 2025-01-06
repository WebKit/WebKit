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

#include "CSSCalcTree.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace CSSCalc {

// MARK: Traversal

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
        void operator()(const AtomString& root)
        {
            functor(root);
        }
        void operator()(const MQ::MediaProgressProviding* root)
        {
            functor(root);
        }
        void operator()(const CQ::ContainerProgressProviding* root)
        {
            functor(root);
        }
    };
    auto caller = Caller { functor };
    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, root);
}

template<typename F> void forAllChildren(const Child& root, const F& functor)
{
    WTF::switchOn(root, [&](const auto& root) { forAllChildren(*root, functor); });
}

// MARK: - forAllChildNodes

// `forAllChildNodes` will call the provided `functor` on all direct `Child` typed children of the provided node. If a child is of type `ChildOrNone` or `std::optional<Child>`, the functor will be called on the unwrapped `Child` if and only if that is what the type is holding.

template<typename F, Leaf Op> void forAllChildNodes(const Op&, const F&)
{
    // No children.
}

template<typename F, typename Op> void forAllChildNodes(const Op& root, const F& functor)
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
            if (root)
                functor(*root);
        }
        void operator()(const ChildOrNone& root)
        {
            WTF::switchOn(root,
                [&](const Child& root) { functor(root); },
                [&](const CSS::NoneRaw&) { }
            );
        }
        void operator()(const Child& root)
        {
            functor(root);
        }
        void operator()(const AtomString&)
        {
        }
        void operator()(const MQ::MediaProgressProviding*)
        {
        }
        void operator()(const CQ::ContainerProgressProviding*)
        {
        }
    };
    auto caller = Caller { functor };
    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, root);
}

template<typename F> void forAllChildNodes(const Child& root, const F& functor)
{
    WTF::switchOn(root, [&](const auto& root) { forAllChildNodes(*root, functor); });
}

} // namespace CSSCalc
} // namespace WebCore
