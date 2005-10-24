/*
    Copyright(C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                 2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

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
#include "RectImpl.h"
#include "CSSPrimitiveValueImpl.h"

using namespace KDOM;

RectImpl::RectImpl(CDFInterface *interface) : Shared<RectImpl>(), m_interface(interface)
{
    m_top = 0;
    m_right = 0;
    m_bottom = 0;
    m_left = 0;
}

RectImpl::~RectImpl()
{
    if(m_top)
        m_top->deref();
    if(m_right)
        m_right->deref();
    if(m_bottom)
        m_bottom->deref();
    if(m_left)
        m_left->deref();
}

void RectImpl::setTop(CSSPrimitiveValueImpl *top)
{
    KDOM_SAFE_SET(m_top, top);
}

void RectImpl::setRight(CSSPrimitiveValueImpl *right)
{
    KDOM_SAFE_SET(m_right, right);
}

void RectImpl::setBottom(CSSPrimitiveValueImpl *bottom)
{
    KDOM_SAFE_SET(m_bottom, bottom);
}

void RectImpl::setLeft(CSSPrimitiveValueImpl *left)
{
    KDOM_SAFE_SET(m_left, left);
}

// vim:ts=4:noet
