/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameSetElement_h
#define HTMLFrameSetElement_h

#include "HTMLElement.h"
#include <memory>

namespace WebCore {

class HTMLFrameSetElement final : public HTMLElement {
public:
    static PassRefPtr<HTMLFrameSetElement> create(const QualifiedName&, Document&);

    bool hasFrameBorder() const { return m_frameborder; }
    bool noResize() const { return m_noresize; }

    int totalRows() const { return m_totalRows; }
    int totalCols() const { return m_totalCols; }
    int border() const { return hasFrameBorder() ? m_border : 0; }

    bool hasBorderColor() const { return m_borderColorSet; }

    const Length* rowLengths() const { return m_rowLengths.get(); }
    const Length* colLengths() const { return m_colLengths.get(); }

    static HTMLFrameSetElement* findContaining(Element* descendant);

    // Declared virtual in Element
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(blur);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(focus);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(load);

    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(beforeunload);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(hashchange);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(offline);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(online);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(popstate);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(resize);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(storage);
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(unload);
#if ENABLE(ORIENTATION_EVENTS)
    DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(orientationchange);
#endif

private:
    HTMLFrameSetElement(const QualifiedName&, Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    virtual void willAttachRenderers() override;
    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    
    virtual void defaultEventHandler(Event*) override;

    virtual bool willRecalcStyle(Style::Change) override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    std::unique_ptr<Length[]> m_rowLengths;
    std::unique_ptr<Length[]> m_colLengths;

    int m_totalRows;
    int m_totalCols;
    
    int m_border;
    bool m_borderSet;
    
    bool m_borderColorSet;

    bool m_frameborder;
    bool m_frameborderSet;
    bool m_noresize;
};

NODE_TYPE_CASTS(HTMLFrameSetElement)

} // namespace WebCore

#endif // HTMLFrameSetElement_h
