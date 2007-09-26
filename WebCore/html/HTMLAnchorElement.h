/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef HTMLAnchorElement_h
#define HTMLAnchorElement_h

#include "HTMLElement.h"

namespace WebCore {

class String;

class HTMLAnchorElement : public HTMLElement {
public:
    HTMLAnchorElement(Document*);
    HTMLAnchorElement(const QualifiedName&, Document*);
    ~HTMLAnchorElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool supportsFocus() const;
    virtual bool isMouseFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isFocusable() const;
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void defaultEventHandler(Event*);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void accessKeyAction(bool fullAction);
    virtual bool isURLAttribute(Attribute*) const;

    virtual bool canStartSelection() const;

    String accessKey() const;
    void setAccessKey(const String&);

    String charset() const;
    void setCharset(const String&);

    String coords() const;
    void setCoords(const String&);

    String href() const;
    void setHref(const String&);

    String hreflang() const;
    void setHreflang(const String&);

    String name() const;
    void setName(const String&);

    String rel() const;
    void setRel(const String&);

    String rev() const;
    void setRev(const String&);

    String shape() const;
    void setShape(const String&);

    void setTabIndex(int);

    virtual String target() const;
    void setTarget(const String&);

    String type() const;
    void setType(const String&);

    String hash() const;
    String host() const;
    String hostname() const;
    String pathname() const;
    String port() const;
    String protocol() const;
    String search() const;
    String text() const;
    
    String toString() const;

    bool isLiveLink() const;

private:
    Element* m_rootEditableElementForSelectionOnMouseDown;
    bool m_wasShiftKeyDownOnMouseDown;
};

} // namespace WebCore

#endif // HTMLAnchorElement_h
