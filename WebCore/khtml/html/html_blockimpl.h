/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

// -------------------------------------------------------------------------
#ifndef HTML_BLOCKIMPL_H
#define HTML_BLOCKIMPL_H

#include "html_elementimpl.h"
#include "dtd.h"
#include "rendering/render_style.h"

namespace DOM {

class DOMString;

class HTMLBlockquoteElementImpl : public HTMLElementImpl
{
public:
    HTMLBlockquoteElementImpl(DocumentPtr *doc);

    ~HTMLBlockquoteElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    void attach();
};

// -------------------------------------------------------------------------

class DOMString;

class HTMLDivElementImpl : public HTMLElementImpl
{
public:
    HTMLDivElementImpl(DocumentPtr *doc);

    ~HTMLDivElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    void parseAttribute(AttrImpl *token);
};

// -------------------------------------------------------------------------

class HTMLHRElementImpl : public HTMLElementImpl
{
public:
    HTMLHRElementImpl(DocumentPtr *doc);

    ~HTMLHRElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

    virtual void parseAttribute(AttrImpl *);

    virtual void attach();
protected:
    bool noShade : 1;
};

// -------------------------------------------------------------------------

class HTMLHeadingElementImpl : public HTMLElementImpl
{
public:
    HTMLHeadingElementImpl(DocumentPtr *doc, ushort _tagid);

    ~HTMLHeadingElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;

protected:
    ushort _id;
};

// -------------------------------------------------------------------------

/*
 * were not using HTMLElementImpl as parent class, since a
 * paragraph should be able to flow around aligned objects. Thus
 * a <p> element has to be inline, and is rendered by
 * HTMLBlockImpl::calcParagraph
 */
class HTMLParagraphElementImpl : public HTMLElementImpl
{
public:
    HTMLParagraphElementImpl(DocumentPtr *doc);

    ~HTMLParagraphElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLPreElementImpl : public HTMLElementImpl
{
public:
    HTMLPreElementImpl(DocumentPtr *doc);

    ~HTMLPreElementImpl();

    long width() const;
    void setWidth( long w );

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
};

// -------------------------------------------------------------------------

class HTMLLayerElementImpl : public HTMLDivElementImpl
{
public:
    HTMLLayerElementImpl( DocumentPtr *doc );
    ~HTMLLayerElementImpl();

    virtual const DOMString nodeName() const;
    virtual ushort id() const;
    
    virtual void parseAttribute(AttrImpl *);

    bool fixed;
};

}; //namespace
#endif
