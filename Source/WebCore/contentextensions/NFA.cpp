/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NFA.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include <wtf/ASCIICType.h>
#include <wtf/DataLog.h>

namespace WebCore {

namespace ContentExtensions {

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
void NFA::debugPrintDot() const
{
    dataLogF("digraph NFA_Transitions {\n");
    dataLogF("    rankdir=LR;\n");
    dataLogF("    node [shape=circle];\n");
    dataLogF("    {\n");

    for (unsigned i = 0; i < nodes.size(); ++i) {
        dataLogF("         %d [label=<Node %d", i, i);

        const auto& node = nodes[i];

        if (node.actionStart != node.actionEnd) {
            dataLogF("<BR/>(Final: ");
            bool isFirst = true;
            for (unsigned actionIndex = node.actionStart; actionIndex < node.actionEnd; ++actionIndex) {
                if (!isFirst)
                    dataLogF(", ");
                dataLogF("%" PRIu64, actions[actionIndex]);
                isFirst = false;
            }
            dataLogF(")");
        }
        dataLogF(">]");

        if (node.actionStart != node.actionEnd)
            dataLogF(" [shape=doublecircle]");
        dataLogF(";\n");
    }
    dataLogF("    }\n");

    dataLogF("    {\n");
    for (unsigned i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];

        HashMap<uint32_t, Vector<uint32_t>, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> transitionsPerTarget;

        for (uint32_t transitionIndex = node.rangesStart; transitionIndex < node.rangesEnd; ++transitionIndex) {
            const ImmutableCharRange& range = transitions[transitionIndex];
            for (uint32_t targetIndex = range.targetStart; targetIndex < range.targetEnd; ++targetIndex) {
                uint32_t target = targets[targetIndex];
                auto addResult = transitionsPerTarget.add(target, Vector<uint32_t>());
                addResult.iterator->value.append(transitionIndex);
            }
        }

        for (const auto& slot : transitionsPerTarget) {
            dataLogF("        %d -> %d [label=\"", i, slot.key);

            bool isFirst = true;
            for (uint32_t rangeIndex : slot.value) {
                if (!isFirst)
                    dataLogF(", ");
                else
                    isFirst = false;

                const ImmutableCharRange& range = transitions[rangeIndex];
                if (range.first == range.last) {
                    if (isASCIIPrintable(range.first))
                    dataLogF("%c", range.first);
                    else
                    dataLogF("%d", range.first);
                } else {
                    if (isASCIIPrintable(range.first) && isASCIIPrintable(range.last))
                    dataLogF("%c-%c", range.first, range.last);
                    else
                    dataLogF("%d-%d", range.first, range.last);
                }
            }
            dataLogF("\"]\n");
        }

        for (uint32_t epsilonTargetIndex = node.epsilonTransitionTargetsStart; epsilonTargetIndex < node.epsilonTransitionTargetsEnd; ++epsilonTargetIndex) {
            uint32_t target = epsilonTransitionsTargets[epsilonTargetIndex];
            dataLogF("        %d -> %d [label=\"ɛ\"]\n", i, target);
        }
    }

    dataLogF("    }\n");
    dataLogF("}\n");
}
#endif

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
