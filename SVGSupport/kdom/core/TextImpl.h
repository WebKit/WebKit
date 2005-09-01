/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_TextImpl_H
#define KDOM_TextImpl_H

#include <kdom/core/CharacterDataImpl.h>

namespace KDOM
{
    class TextImpl : public CharacterDataImpl
    {
    public:
        TextImpl(DocumentPtr *doc, DOMStringImpl *text);
        virtual ~TextImpl();

        virtual DOMStringImpl *nodeName() const;
        virtual unsigned short nodeType() const;

        TextImpl *splitText(unsigned long offset);

        bool isElementContentWhitespace() const;
        DOMStringImpl *wholeText() const;
        TextImpl *replaceWholeText(DOMStringImpl *content);

        // Internal
        virtual NodeImpl *cloneNode(bool deep, DocumentPtr *doc) const;

    protected:
        QPtrList<NodeImpl> logicallyAdjacentTextNodes() const;
        bool checkChildren(NodeImpl *node, const QPtrList<NodeImpl> &removables) const;
    };
};

#endif

// vim:ts=4:noet
