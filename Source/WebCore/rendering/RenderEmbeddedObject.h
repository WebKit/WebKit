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

#pragma once

#include "RenderWidget.h"

namespace WebCore {

class HTMLAppletElement;
class MouseEvent;
class TextRun;

// Renderer for embeds and objects, often, but not always, rendered via plug-ins.
// For example, <embed src="foo.html"> does not invoke a plug-in.
class RenderEmbeddedObject : public RenderWidget {
    WTF_MAKE_ISO_ALLOCATED(RenderEmbeddedObject);
public:
    RenderEmbeddedObject(HTMLFrameOwnerElement&, RenderStyle&&);
    virtual ~RenderEmbeddedObject();

    static RenderPtr<RenderEmbeddedObject> createForApplet(HTMLAppletElement&, RenderStyle&&);

    enum PluginUnavailabilityReason {
        PluginMissing,
        PluginCrashed,
        PluginBlockedByContentSecurityPolicy,
        InsecurePluginVersion,
        UnsupportedPlugin,
        PluginTooSmall
    };
    PluginUnavailabilityReason pluginUnavailabilityReason() const { return m_pluginUnavailabilityReason; };
    WEBCORE_EXPORT void setPluginUnavailabilityReason(PluginUnavailabilityReason);
    WEBCORE_EXPORT void setPluginUnavailabilityReasonWithDescription(PluginUnavailabilityReason, const String& description);

    bool isPluginUnavailable() const { return m_isPluginUnavailable; }

    WEBCORE_EXPORT void setUnavailablePluginIndicatorIsHidden(bool);

    void handleUnavailablePluginIndicatorEvent(Event*);

    bool allowsAcceleratedCompositing() const;

    LayoutRect unavailablePluginIndicatorBounds(const LayoutPoint& accumulatedOffset) const;

    const String& pluginReplacementTextIfUnavailable() const { return m_unavailablePluginReplacementText; }

protected:
    void paintReplaced(PaintInfo&, const LayoutPoint&) final;
    void paint(PaintInfo&, const LayoutPoint&) override;

    CursorDirective getCursor(const LayoutPoint&, Cursor&) const override;

protected:
    void layout() override;
    void willBeDestroyed() override;

private:
    const char* renderName() const override { return "RenderEmbeddedObject"; }
    bool isEmbeddedObject() const final { return true; }

    bool showsUnavailablePluginIndicator() const { return isPluginUnavailable() && m_isUnavailablePluginIndicatorState != UnavailablePluginIndicatorState::Hidden; }
    void paintSnapshotImage(PaintInfo&, const LayoutPoint&, Image&);
    void paintContents(PaintInfo&, const LayoutPoint&) final;

    bool requiresLayer() const final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Element** stopElement = nullptr, RenderBox* startBox = nullptr, const IntPoint& wheelEventAbsolutePoint = IntPoint()) final;
    bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier, Element** stopElement) final;

    void setUnavailablePluginIndicatorIsPressed(bool);
    bool isInUnavailablePluginIndicator(const MouseEvent&) const;
    bool isInUnavailablePluginIndicator(const FloatPoint&) const;
    void getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, FloatRect& indicatorRect, FloatRect& replacementTextRect, FloatRect& arrowRect, FontCascade&, TextRun&, float& textWidth) const;
    LayoutRect getReplacementTextGeometry(const LayoutPoint& accumulatedOffset) const;

    bool canHaveChildren() const final;
    virtual bool canHaveWidget() const { return true; }

    bool m_isPluginUnavailable;
    enum class UnavailablePluginIndicatorState { Uninitialized, Hidden, Visible };
    UnavailablePluginIndicatorState m_isUnavailablePluginIndicatorState { UnavailablePluginIndicatorState::Uninitialized };
    PluginUnavailabilityReason m_pluginUnavailabilityReason;
    String m_unavailablePluginReplacementText;
    bool m_unavailablePluginIndicatorIsPressed;
    bool m_mouseDownWasInUnavailablePluginIndicator;
    String m_unavailabilityDescription;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderEmbeddedObject, isEmbeddedObject())
