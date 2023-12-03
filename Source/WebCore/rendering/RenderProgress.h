/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "RenderBlockFlow.h"

namespace WebCore {

class HTMLProgressElement;

class RenderProgress final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderProgress);
public:
    RenderProgress(HTMLElement&, RenderStyle&&);
    virtual ~RenderProgress();

    double position() const { return m_position; }
    double animationProgress() const;
    MonotonicTime animationStartTime() const { return m_animationStartTime; }

    bool isDeterminate() const;
    void updateFromElement() override;

    HTMLProgressElement* progressElement() const;

private:
    ASCIILiteral renderName() const override { return "RenderProgress"_s; }
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    void animationTimerFired();
    void updateAnimationState();

    double m_position;
    MonotonicTime m_animationStartTime;
    bool m_animating { false };
    Timer m_animationTimer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderProgress, isRenderProgress())
