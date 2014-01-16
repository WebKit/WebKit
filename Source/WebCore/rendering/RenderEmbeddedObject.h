/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef RenderEmbeddedObject_h
#define RenderEmbeddedObject_h

#include "RenderWidget.h"

namespace WebCore {

class HTMLAppletElement;
class MouseEvent;
class TextRun;

// Renderer for embeds and objects, often, but not always, rendered via plug-ins.
// For example, <embed src="foo.html"> does not invoke a plug-in.
class RenderEmbeddedObject : public RenderWidget {
public:
    RenderEmbeddedObject(HTMLFrameOwnerElement&, PassRef<RenderStyle>);
    virtual ~RenderEmbeddedObject();

    static RenderPtr<RenderEmbeddedObject> createForApplet(HTMLAppletElement&, PassRef<RenderStyle>);

    enum PluginUnavailabilityReason {
        PluginMissing,
        PluginCrashed,
        PluginBlockedByContentSecurityPolicy,
        InsecurePluginVersion,
    };
    void setPluginUnavailabilityReason(PluginUnavailabilityReason);
    void setPluginUnavailabilityReasonWithDescription(PluginUnavailabilityReason, const String& description);

    bool isPluginUnavailable() const { return m_isPluginUnavailable; }
    bool showsUnavailablePluginIndicator() const { return isPluginUnavailable() && !m_isUnavailablePluginIndicatorHidden; }

    void setUnavailablePluginIndicatorIsHidden(bool);

    void handleUnavailablePluginIndicatorEvent(Event*);

    bool isReplacementObscured() const;

#if USE(ACCELERATED_COMPOSITING)
    bool allowsAcceleratedCompositing() const;
#endif

protected:
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) override FINAL;
    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const override;

protected:
    virtual void layout() override;

private:
    virtual const char* renderName() const override { return "RenderEmbeddedObject"; }
    virtual bool isEmbeddedObject() const override FINAL { return true; }

    void paintSnapshotImage(PaintInfo&, const LayoutPoint&, Image*);
    virtual void paintContents(PaintInfo&, const LayoutPoint&) override FINAL;

#if USE(ACCELERATED_COMPOSITING)
    virtual bool requiresLayer() const override FINAL;
#endif

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override FINAL;

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier, Element** stopElement) override FINAL;
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier, Element** stopElement) override FINAL;

    void setUnavailablePluginIndicatorIsPressed(bool);
    bool isInUnavailablePluginIndicator(MouseEvent*) const;
    bool isInUnavailablePluginIndicator(const LayoutPoint&) const;
    bool getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, FloatRect& indicatorRect, FloatRect& replacementTextRect, FloatRect& arrowRect, Font&, TextRun&, float& textWidth) const;
    LayoutRect unavailablePluginIndicatorBounds(const LayoutPoint&) const;

    virtual bool canHaveChildren() const override FINAL;
    virtual bool canHaveWidget() const { return true; }

    bool m_isPluginUnavailable;
    bool m_isUnavailablePluginIndicatorHidden;
    PluginUnavailabilityReason m_pluginUnavailabilityReason;
    String m_unavailablePluginReplacementText;
    bool m_unavailablePluginIndicatorIsPressed;
    bool m_mouseDownWasInUnavailablePluginIndicator;
    String m_unavailabilityDescription;
};

RENDER_OBJECT_TYPE_CASTS(RenderEmbeddedObject, isEmbeddedObject())

} // namespace WebCore

#endif // RenderEmbeddedObject_h
