/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#if ENABLE(PROGRESS_TAG)

#include "RenderProgress.h"

#include "HTMLDivElement.h"
#include "HTMLProgressElement.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include <wtf/CurrentTime.h>
#include <wtf/RefPtr.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

class ProgressValueElement : public HTMLDivElement {
public:
    ProgressValueElement(Document*, Node* shadowParent);

private:        
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_shadowParent; }

    Node* m_shadowParent;
};

ProgressValueElement::ProgressValueElement(Document* document, Node* shadowParent)
: HTMLDivElement(divTag, document)
, m_shadowParent(shadowParent)
{
}

RenderProgress::RenderProgress(HTMLProgressElement* element)
    : RenderBlock(element)
    , m_position(-1)
    , m_animationStartTime(0)
    , m_animationRepeatInterval(0)
    , m_animationDuration(0)
    , m_animating(false)
    , m_animationTimer(this, &RenderProgress::animationTimerFired)
    , m_valuePart(0)
{
}

RenderProgress::~RenderProgress()
{
    if (m_valuePart)
        m_valuePart->detach();
}

void RenderProgress::layout()
{
    ASSERT(needsLayout());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    IntSize oldSize = size();

    calcWidth();
    calcHeight();
    if (m_valuePart) {
        IntRect valuePartRect(borderLeft() + paddingLeft(), borderTop() + paddingTop(), lround((width() - borderLeft() - paddingLeft() - borderRight() - paddingRight()) * position()), height()  - borderTop() - paddingTop() - borderBottom() - paddingBottom());
        if (style()->direction() == RTL)
            valuePartRect.setX(width() - borderRight() - paddingRight() - valuePartRect.width());
        toRenderBox(m_valuePart->renderer())->setFrameRect(valuePartRect);

        if (oldSize != size())
            m_valuePart->renderer()->setChildNeedsLayout(true, false);
        
        LayoutStateMaintainer statePusher(view(), this, size());
        
        IntRect oldRect = toRenderBox(m_valuePart->renderer())->frameRect();
        
        m_valuePart->renderer()->layoutIfNeeded();

        toRenderBox(m_valuePart->renderer())->setFrameRect(valuePartRect);
        if (m_valuePart->renderer()->checkForRepaintDuringLayout())
            m_valuePart->renderer()->repaintDuringLayoutIfMoved(oldRect);
        
        statePusher.pop();
        addOverflowFromChild(toRenderBox(m_valuePart->renderer()));
    }

    updateAnimationState();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderProgress::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    updateValuePartState();
}
    
void RenderProgress::updateFromElement()
{
    HTMLProgressElement* element = progressElement();
    if (m_position == element->position())
        return;
    m_position = element->position();

    updateAnimationState();

    updateValuePartState();
    
    repaint();
}

void RenderProgress::updateValuePartState()
{
    bool needLayout = !style()->hasAppearance() || m_valuePart;
    if (!style()->hasAppearance() && !m_valuePart) {
        m_valuePart = new ProgressValueElement(document(), node());
        RefPtr<RenderStyle> styleForValuePart = createStyleForValuePart(style());
        m_valuePart->setRenderer(m_valuePart->createRenderer(renderArena(), styleForValuePart.get()));
        m_valuePart->renderer()->setStyle(styleForValuePart.release());
        m_valuePart->setAttached();
        m_valuePart->setInDocument();
        addChild(m_valuePart->renderer());
    } else if (style()->hasAppearance() && m_valuePart) {
        m_valuePart->detach();
        m_valuePart = 0;
    }
    if (needLayout)
        setNeedsLayout(true);
}

PassRefPtr<RenderStyle> RenderProgress::createStyleForValuePart(RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> styleForValuePart;
    RenderStyle* pseudoStyle = getCachedPseudoStyle(PROGRESS_BAR_VALUE);
    if (pseudoStyle)
        styleForValuePart = RenderStyle::clone(pseudoStyle);
    else
        styleForValuePart = RenderStyle::create();
    
    if (parentStyle)
        styleForValuePart->inheritFrom(parentStyle);
    styleForValuePart->setDisplay(BLOCK);
    styleForValuePart->setAppearance(ProgressBarValuePart);
    return styleForValuePart.release();
}

double RenderProgress::animationProgress()
{
    return m_animating ? (fmod((currentTime() - m_animationStartTime), m_animationDuration) / m_animationDuration) : 0;
}

void RenderProgress::animationTimerFired(Timer<RenderProgress>*)
{
    repaint();
}

void RenderProgress::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.phase == PaintPhaseBlockBackground) {
        if (!m_animationTimer.isActive() && m_animating)
            m_animationTimer.startOneShot(m_animationRepeatInterval);
    }

    RenderBlock::paint(paintInfo, tx, ty);
}

void RenderProgress::updateAnimationState()
{
    m_animationDuration = theme()->animationDurationForProgressBar(this);
    m_animationRepeatInterval = theme()->animationRepeatIntervalForProgressBar(this);

    bool animating = style()->hasAppearance() && m_animationDuration > 0;
    if (animating == m_animating)
        return;

    m_animating = animating;
    if (m_animating) {
        m_animationStartTime = currentTime();
        m_animationTimer.startOneShot(m_animationRepeatInterval);
    } else
        m_animationTimer.stop();
}

HTMLProgressElement* RenderProgress::progressElement() const
{
    return static_cast<HTMLProgressElement*>(node());
}    

} // namespace WebCore
#endif
