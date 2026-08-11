/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutUnit.h>

namespace WebCore {
namespace Layout {

struct AxisConstraint {
    // https://drafts.csswg.org/css-grid-1/#algo-flex-tracks
    enum class FreeSpaceScenario : uint8_t {
        MinContent,
        Definite,
        MaxContent
    };


    static AxisConstraint minContent(std::optional<LayoutUnit> containerMinSize = std::nullopt, std::optional<LayoutUnit> containerMaxSize = std::nullopt)
    {
        return AxisConstraint(FreeSpaceScenario::MinContent, 0_lu, containerMinSize, containerMaxSize);
    }

    static AxisConstraint maxContent(std::optional<LayoutUnit> containerMinSize = std::nullopt, std::optional<LayoutUnit> containerMaxSize = std::nullopt)
    {
        return AxisConstraint(FreeSpaceScenario::MaxContent, 0_lu, containerMinSize, containerMaxSize);
    }

    static AxisConstraint definite(LayoutUnit space, std::optional<LayoutUnit> containerMinSize = std::nullopt, std::optional<LayoutUnit> containerMaxSize = std::nullopt)
    {
        return AxisConstraint(FreeSpaceScenario::Definite, space, containerMinSize, containerMaxSize);
    }

    FreeSpaceScenario scenario() const { return m_scenario; }

    // Returns available space for Definite constraints.
    // Caller must check scenario() first - only valid when scenario() == FreeSpaceScenario::Definite.
    LayoutUnit availableSpace() const
    {
        ASSERT(m_scenario == FreeSpaceScenario::Definite);
        return m_space;
    }

    // Container size constraints (orthogonal to constraint scenario)
    std::optional<LayoutUnit> containerMinimumSize() const { return m_containerMinimumSize; }
    std::optional<LayoutUnit> containerMaximumSize() const { return m_containerMaximumSize; }

private:
    AxisConstraint(FreeSpaceScenario scenario, LayoutUnit space, std::optional<LayoutUnit> containerMinSize, std::optional<LayoutUnit> containerMaxSize)
        : m_scenario(scenario)
        , m_space(space)
        , m_containerMinimumSize(containerMinSize)
        , m_containerMaximumSize(containerMaxSize)
    {
        // Disallow negative available space for Definite scenario.
        ASSERT(scenario != FreeSpaceScenario::Definite || space >= 0_lu);
    }

    FreeSpaceScenario m_scenario;
    LayoutUnit m_space; // Only valid when m_scenario == Definite
    std::optional<LayoutUnit> m_containerMinimumSize;
    std::optional<LayoutUnit> m_containerMaximumSize;
};

} // namespace Layout
} // namespace WebCore
