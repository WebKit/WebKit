/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KDOM_XPointer_XPointerExpressionImpl_H
#define KDOM_XPointer_XPointerExpressionImpl_H

#include <qvaluelist.h>

#include <kdom/Shared.h>

namespace KDOM
{

class NodeImpl;
class DocumentImpl;
class DOMStringImpl;

namespace XPointer
{
    class PointerPartImpl;
    class XPointerResultImpl;

    class XPointerExpressionImpl : public Shared
    {
    public:
        typedef QValueList<PointerPartImpl *> List;

        XPointerExpressionImpl(DOMStringImpl *raw, NodeImpl *relatedNode, DocumentImpl *context);
        virtual ~XPointerExpressionImpl();

        XPointerResultImpl *evaluate() const;

        DOMStringImpl *string() const;

        /**
         * Determines whether the expression is a ShortHand pointer.
         *
         * @returns true if the only component is a ShortHand
         */
        bool isShortHand() const;
        void setIsShortHand(bool state);

        void appendPart(PointerPartImpl *part);
        
        List pointerParts() const;

    private:
        bool m_isShortHand : 1;
        DOMStringImpl *m_pointer;

        List m_parts;
        NodeImpl *m_relatedNode;
        DocumentImpl *m_context;
    };
};

};

#endif

// vim:ts=3:noet
