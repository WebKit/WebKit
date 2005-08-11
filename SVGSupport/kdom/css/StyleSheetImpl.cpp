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

#include "DocumentImpl.h"
#include "MediaListImpl.h"
#include "StyleSheetImpl.h"

using namespace KDOM;

StyleSheetImpl::StyleSheetImpl(StyleSheetImpl *parentSheet, const DOMString &href) : StyleListImpl(parentSheet)
{
	m_media = 0;
	m_parentNode = 0;
	m_strHref = href;
	m_disabled = false;
}

StyleSheetImpl::StyleSheetImpl(NodeImpl *parentNode, const DOMString &href) : StyleListImpl()
{
	m_media = 0;
	m_strHref = href;
	m_disabled = false;
	m_parentNode = parentNode;
}

StyleSheetImpl::StyleSheetImpl(StyleBaseImpl *owner, DOMString href) : StyleListImpl(owner)
{
	m_media = 0;
	m_strHref = href;
	m_disabled = false;
	m_parentNode = 0;
}

StyleSheetImpl::~StyleSheetImpl()
{
	if(m_media)
	{
		m_media->setParent(0);
		m_media->deref();
	}
}

void StyleSheetImpl::setDisabled(bool disabled)
{
	bool updateStyle = isCSSStyleSheet() && m_parentNode && disabled != m_disabled;
	m_disabled = disabled;
	if(updateStyle)
		m_parentNode->ownerDocument()->updateStyleSelector();
}

bool StyleSheetImpl::disabled() const
{
	return m_disabled;
}

NodeImpl *StyleSheetImpl::ownerNode() const
{
	return m_parentNode;
}

StyleSheetImpl *StyleSheetImpl::parentStyleSheet() const
{
	if(m_parent && m_parent->isStyleSheet())
		return static_cast<StyleSheetImpl *>(m_parent);

	return 0;
}

DOMString StyleSheetImpl::href() const
{
	return m_strHref;
}

DOMString StyleSheetImpl::title() const
{
	return m_strTitle;
}

MediaListImpl *StyleSheetImpl::media() const
{
	return m_media;
}

void StyleSheetImpl::setTitle(const DOMString &title)
{
	m_strTitle = title;
}

void StyleSheetImpl::setMedia(MediaListImpl *media)
{
	KDOM_SAFE_SET(m_media, media);
}

// vim:ts=4:noet
