/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
 * $Id$
 */
#ifndef HTML_INLINEIMPL_H
#define HTML_INLINEIMPL_H

#include "html_elementimpl.h"

namespace DOM {

class DOMString;

class HTMLAnchorElementImpl : public HTMLElementImpl
{
public:
    HTMLAnchorElementImpl(DocumentPtr *doc);

    virtual ~HTMLAnchorElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    DOMString areaHref() const { return href; }
    DOMString targetRef() const { return target; }

    virtual bool isSelectable() const { return href!=0; }

    virtual void parseAttribute(AttrImpl *attr);
    virtual bool prepareMouseEvent( int x, int y,
                                    int _tx, int _ty,
                                    MouseEvent *ev);
    virtual void defaultEventHandler(EventImpl *evt);
protected:
    DOMStringImpl *href;
    DOMStringImpl *target;
};

// -------------------------------------------------------------------------

typedef enum { BRNone=0, BRLeft, BRRight, BRAll} BRClear;

class HTMLBRElementImpl : public HTMLElementImpl
{
public:

    HTMLBRElementImpl(DocumentPtr *doc);

    ~HTMLBRElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);

    virtual void attach();
};

// -------------------------------------------------------------------------

class HTMLFontElementImpl : public HTMLElementImpl
{
public:
    HTMLFontElementImpl(DocumentPtr *doc);

    ~HTMLFontElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *attr);
    void attach();
};

// -------------------------------------------------------------------------

class HTMLModElementImpl : public HTMLElementImpl
{
public:
    HTMLModElementImpl(DocumentPtr *doc, ushort tagid);

    ~HTMLModElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

protected:
    ushort _id;
};

// -------------------------------------------------------------------------

class DOMString;

class HTMLQuoteElementImpl : public HTMLElementImpl
{
public:
    HTMLQuoteElementImpl(DocumentPtr *doc);

    ~HTMLQuoteElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

}; //namespace

#endif
