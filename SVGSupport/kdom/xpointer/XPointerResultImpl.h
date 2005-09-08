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

#ifndef KDOM_XPointer_XPointerResultImpl_H
#define KDOM_XPointer_XPointerResultImpl_H

#include <kdom/Shared.h>
#include <kdom/xpointer/kdomxpointer.h>

namespace KDOM
{

class NodeImpl;

namespace XPointer
{
    class XPointerResultImpl : public Shared
    {
    public:
        XPointerResultImpl(const ResultType code);
        virtual ~XPointerResultImpl();

        NodeImpl *singleNodeValue() const;
        void setSingleNodeValue(NodeImpl *node);

        ResultType resultType() const;
        void setResultType(const ResultType code);
        void setResultType(const unsigned short code);

    private:
        NodeImpl *m_single;
        ResultType m_resultType;
    };
};

};

#endif

// vim:ts=4:noet
