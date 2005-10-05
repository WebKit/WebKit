/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
              (C) 2001 Peter Kelly (pmk@post.com)

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

#include "config.h"
#include "NodeFilterImpl.h"

using namespace KDOM;

NodeFilterImpl::NodeFilterImpl(NodeFilterCondition *condition)
: Shared(), m_condition(condition)
{
    if(m_condition)
        m_condition->ref();
}

NodeFilterImpl::~NodeFilterImpl()
{
    if(m_condition)
        m_condition->deref();
}

short NodeFilterImpl::acceptNode(NodeImpl *node) const
{
    // cast to short silences "enumeral and non-enumeral types in return" warning
    return m_condition ? m_condition->acceptNode(node) : static_cast<short>(FILTER_ACCEPT);

}

// vim:ts=4:noet
