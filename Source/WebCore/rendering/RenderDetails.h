/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef RenderDetails_h
#define RenderDetails_h

#include "RenderFlexibleBox.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderDetailsMarker;

class RenderDetails : public RenderBlock {
public:
    explicit RenderDetails(Node*);

    virtual const char* renderName() const { return "RenderDetails"; }
    virtual bool isDetails() const { return true; }

    bool isOpen() const;
    IntRect interactiveArea() const { return m_interactiveArea; }
    void markerDestroyed();
    void summaryDestroyed(RenderObject*);

private:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) { }
    virtual bool createsAnonymousWrapper() const { return true; }

    virtual bool requiresForcedStyleRecalcPropagation() const { return true; }

    virtual void layout();
    virtual void destroy();

    void createDefaultSummary();
    void replaceMainSummary(RenderObject*);
    void moveSummaryToContents();
    void checkMainSummary();
    RenderObject* getRenderPosition(RenderObject*);
    PassRefPtr<RenderStyle> createSummaryStyle();
    void setMarkerStyle();

    RenderBlock* summaryBlock();
    RenderBlock* contentBlock();

    RenderObject* getParentOfFirstLineBox(RenderBlock* curr);
    RenderObject* firstNonMarkerChild(RenderObject* parent);
    void updateMarkerLocation();

    RenderBlock* m_summaryBlock;
    RenderBlock* m_contentBlock;

    RenderObject* m_defaultSummaryBlock;
    RenderObject* m_defaultSummaryText;

    IntRect m_interactiveArea;

    RenderDetailsMarker* m_marker;

    RenderObject* m_mainSummary;
};

inline RenderDetails* toRenderDetails(RenderObject* object)
{
    ASSERT(!object || object->isDetails());
    return static_cast<RenderDetails*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderDetails(const RenderDetails*);

} // namespace WebCore

#endif // RenderDetails_h
