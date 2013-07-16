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

#include "RenderPart.h"

namespace WebCore {

class MouseEvent;
class TextRun;

class RenderEmbeddedObject;

class RenderEmbeddedObjectEventListener : public EventListener {
public:
    static PassRefPtr<RenderEmbeddedObjectEventListener> create(RenderEmbeddedObject* renderEmbeddedObject) { return adoptRef(new RenderEmbeddedObjectEventListener(renderEmbeddedObject)); }
    static const RenderEmbeddedObjectEventListener* cast(const EventListener* listener)
    {
        return listener->type() == RenderEmbeddedObjectEventListenerType
            ? static_cast<const RenderEmbeddedObjectEventListener*>(listener)
            : 0;
    }

    virtual bool operator==(const EventListener& other);

private:
    RenderEmbeddedObjectEventListener(RenderEmbeddedObject* renderEmbeddedObject)
        : EventListener(RenderEmbeddedObjectEventListenerType)
        , m_renderEmbeddedObject(renderEmbeddedObject)
    {
    }

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    RenderEmbeddedObject* m_renderEmbeddedObject;
};

// Renderer for embeds and objects, often, but not always, rendered via plug-ins.
// For example, <embed src="foo.html"> does not invoke a plug-in.
class RenderEmbeddedObject : public RenderPart {
public:
    RenderEmbeddedObject(Element*);
    virtual ~RenderEmbeddedObject();

    enum PluginUnavailabilityReason {
        PluginMissing,
        PluginCrashed,
        PluginBlockedByContentSecurityPolicy,
        InsecurePluginVersion,
    };
    void setPluginUnavailabilityReason(PluginUnavailabilityReason);
    bool showsUnavailablePluginIndicator() const;

    void setPluginUnavailabilityReasonDescription(const String&);
    const String& pluginUnavailabilityReasonDescription() { return m_unavailabilityDescription; }

    // FIXME: This belongs on HTMLObjectElement.
    bool hasFallbackContent() const { return m_hasFallbackContent; }
    void setHasFallbackContent(bool hasFallbackContent) { m_hasFallbackContent = hasFallbackContent; }

    void handleUnavailablePluginButtonClickEvent(Event*);

    bool isReplacementObscured() const;

#if USE(ACCELERATED_COMPOSITING)
    virtual bool allowsAcceleratedCompositing() const;
#endif

protected:
    virtual void willBeDestroyed() OVERRIDE;

    virtual void paint(PaintInfo&, const LayoutPoint&);

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

protected:
    virtual void layout() OVERRIDE;

private:
    virtual const char* renderName() const { return "RenderEmbeddedObject"; }
    virtual bool isEmbeddedObject() const { return true; }

    void paintSnapshotImage(PaintInfo&, const LayoutPoint&, Image*);
    virtual void paintContents(PaintInfo&, const LayoutPoint&) OVERRIDE;

#if USE(ACCELERATED_COMPOSITING)
    virtual bool requiresLayer() const;
#endif

    virtual void viewCleared();

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier, Node** stopNode);
    virtual bool logicalScroll(ScrollLogicalDirection, ScrollGranularity, float multiplier, Node** stopNode);

    virtual bool canHaveChildren() const;
    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }
    
    virtual bool canHaveWidget() const { return true; }

    bool m_hasFallbackContent; // FIXME: This belongs on HTMLObjectElement.

    bool m_showsUnavailablePluginIndicator;
    PluginUnavailabilityReason m_pluginUnavailabilityReason;
    RenderObjectChildList m_children;
    RefPtr<HTMLElement> m_indicator;
    RefPtr<HTMLElement> m_indicatorLabel;
    String m_unavailabilityDescription;
};

inline RenderEmbeddedObject* toRenderEmbeddedObject(RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isEmbeddedObject());
    return static_cast<RenderEmbeddedObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderEmbeddedObject(const RenderEmbeddedObject*);

} // namespace WebCore

#endif // RenderEmbeddedObject_h
