/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef HTMLLabelElement_h
#define HTMLLabelElement_h

#include "HTMLElement.h"

namespace WebCore {

class HTMLFormElement;
class MappedAttribute;

class HTMLLabelElement : public HTMLElement
{
public:
    HTMLLabelElement(Document *doc);
    virtual ~HTMLLabelElement();

    virtual int tagPriority() const { return 5; }

    virtual bool isFocusable() const;

    virtual void accessKeyAction(bool sendToAnyElement);

    // Overridden to update the hover/active state of the corresponding control.
    virtual void setActive(bool b = true, bool pause = false);
    virtual void setHovered(bool b = true);

    // Overridden to either click() or focus() the corresponding control.
    virtual void defaultEventHandler(Event*);

    /**
     * the form element this label is associated to.
     */
    HTMLElement *formElement();

    HTMLFormElement *form() const;

    String accessKey() const;
    void setAccessKey(const String &);

    String htmlFor() const;
    void setHtmlFor(const String &);

    void focus(bool restorePreviousSelection = true);

    virtual HTMLFormElement* formForEventHandlerScope() const;

 private:
    String m_formElementID;
};

} //namespace

#endif
