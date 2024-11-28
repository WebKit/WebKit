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
#include "CSSCalcTree+ComputedStyleDependencies.h"

#include "CSSCalcTree+Traversal.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPropertyNames.h"
#include "CSSUnits.h"
#include "ComputedStyleDependencies.h"

namespace WebCore {
namespace CSSCalc {

static void collectComputedStyleDependencies(const Child& root, ComputedStyleDependencies& dependencies)
{
    WTF::switchOn(root,
        [&](const Number&) {
            // No potential dependencies.
        },
        [&](const Percentage&) {
            // No potential dependencies.
        },
        [&](const CanonicalDimension&) {
            // No potential dependencies.
        },
        [&](const NonCanonicalDimension& root) {
            CSS::collectComputedStyleDependencies(dependencies, root.unit);
        },
        [&](const Symbol& root) {
            CSS::collectComputedStyleDependencies(dependencies, root.unit);
        },
        [&](const IndirectNode<Anchor>& anchor) {
            dependencies.anchors = true;
            if (anchor->fallback)
                collectComputedStyleDependencies(*anchor->fallback, dependencies);
        },
        [&](const IndirectNode<AnchorSize>& anchorSize) {
            dependencies.anchors = true;
            if (anchorSize->fallback)
                collectComputedStyleDependencies(*anchorSize->fallback, dependencies);
        },
        [&](const auto& root) {
            forAllChildNodes(*root, [&](const auto& root) { collectComputedStyleDependencies(root, dependencies); });
        }
    );
}

void collectComputedStyleDependencies(const Tree& tree, ComputedStyleDependencies& dependencies)
{
    collectComputedStyleDependencies(tree.root, dependencies);
}

} // namespace CSSCalc
} // namespace WebCore
