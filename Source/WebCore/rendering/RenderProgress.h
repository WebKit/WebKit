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

#ifndef RenderProgress_h
#define RenderProgress_h

#if ENABLE(PROGRESS_ELEMENT)
#include "RenderBlockFlow.h"

namespace WebCore {

class HTMLProgressElement;

class RenderProgress final : public RenderBlockFlow {
public:
    RenderProgress(HTMLElement&, PassRef<RenderStyle>);
    virtual ~RenderProgress();

    double position() const { return m_position; }
    double animationProgress() const;
    double animationStartTime() const { return m_animationStartTime; }

    bool isDeterminate() const;
    virtual void updateFromElement() override;

    HTMLProgressElement* progressElement() const;

private:
    virtual const char* renderName() const override { return "RenderProgress"; }
    virtual bool isProgress() const override { return true; }
    virtual bool requiresForcedStyleRecalcPropagation() const override { return true; }
    virtual bool canBeReplacedWithInlineRunIn() const override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    void animationTimerFired(Timer<RenderProgress>&);
    void updateAnimationState();

    double m_position;
    double m_animationStartTime;
    double m_animationRepeatInterval;
    double m_animationDuration;
    bool m_animating;
    Timer<RenderProgress> m_animationTimer;
};

RENDER_OBJECT_TYPE_CASTS(RenderProgress, isProgress())

} // namespace WebCore

#endif

#endif // RenderProgress_h

