/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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
#include "Color.h"

namespace WebCore {

class HTMLFrameSetElement : public HTMLElement {
public:
    HTMLFrameSetElement(const QualifiedName&, Document*);
    ~HTMLFrameSetElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    virtual bool checkDTD(const Node* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    
    virtual void defaultEventHandler(Event*);

    bool hasFrameBorder() const { return frameborder; }
    bool noResize() const { return noresize; }

    int totalRows() const { return m_totalRows; }
    int totalCols() const { return m_totalCols; }
    int border() const { return m_border; }

    bool hasBorderColor() const { return m_borderColorSet; }

    virtual void recalcStyle(StyleChange);
    
    String cols() const;
    void setCols(const String&);

    String rows() const;
    void setRows(const String&);

    const Length* rowLengths() const { return m_rows; }
    const Length* colLengths() const { return m_cols; }

    // Event handler attributes
    virtual EventListener* onblur() const;
    virtual void setOnblur(PassRefPtr<EventListener>);
    virtual EventListener* onerror() const;
    virtual void setOnerror(PassRefPtr<EventListener>);
    virtual EventListener* onfocus() const;
    virtual void setOnfocus(PassRefPtr<EventListener>);
    virtual EventListener* onload() const;
    virtual void setOnload(PassRefPtr<EventListener>);

    EventListener* onbeforeunload() const;
    void setOnbeforeunload(PassRefPtr<EventListener>);
    EventListener* onhashchange() const;
    void setOnhashchange(PassRefPtr<EventListener>);
    EventListener* onmessage() const;
    void setOnmessage(PassRefPtr<EventListener>);
    EventListener* onoffline() const;
    void setOnoffline(PassRefPtr<EventListener>);
    EventListener* ononline() const;
    void setOnonline(PassRefPtr<EventListener>);
#if ENABLE(ORIENTATION_EVENTS)
    EventListener* onorientationchange() const;
    void setOnorientationchange(PassRefPtr<EventListener>);
#endif
    EventListener* onresize() const;
    void setOnresize(PassRefPtr<EventListener>);
    EventListener* onstorage() const;
    void setOnstorage(PassRefPtr<EventListener>);
    EventListener* onunload() const;
    void setOnunload(PassRefPtr<EventListener>);

private:
    Length* m_rows;
    Length* m_cols;

    int m_totalRows;
    int m_totalCols;
    
    int m_border;
    bool m_borderSet;
    
    bool m_borderColorSet;

    bool frameborder;
    bool frameBorderSet;
    bool noresize;
};

} // namespace WebCore

#endif // HTMLFrameSetElement_h
