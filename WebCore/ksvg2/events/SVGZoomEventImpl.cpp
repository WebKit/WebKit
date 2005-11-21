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

#include "config.h"
#include "SVGRectImpl.h"
#include "SVGPointImpl.h"
#include "SVGZoomEventImpl.h"

using namespace KSVG;

SVGZoomEventImpl::SVGZoomEventImpl()
{
    m_newScale = 0.0;
    m_previousScale = 0.0;

    m_zoomRectScreen = 0;

    m_newTranslate = 0;
    m_previousTranslate = 0;
}

SVGZoomEventImpl::~SVGZoomEventImpl()
{
    if(m_newTranslate)
        m_newTranslate->deref();
    if(m_previousTranslate)
        m_previousTranslate->deref();
    if(m_zoomRectScreen)
        m_zoomRectScreen->deref();
}

SVGRectImpl *SVGZoomEventImpl::zoomRectScreen() const
{
    return m_zoomRectScreen;
}

float SVGZoomEventImpl::previousScale() const
{
    return m_previousScale;
}

void SVGZoomEventImpl::setPreviousScale(float scale)
{
    m_previousScale = scale;
}

SVGPointImpl *SVGZoomEventImpl::previousTranslate() const
{
    return m_previousTranslate;
}

float SVGZoomEventImpl::newScale() const
{
    return m_newScale;
}

void SVGZoomEventImpl::setNewScale(float scale)
{
    m_newScale = scale;
}

SVGPointImpl *SVGZoomEventImpl::newTranslate() const
{
    return m_newTranslate;
}

// vim:ts=4:noet
