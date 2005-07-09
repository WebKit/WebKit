/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include <qstringlist.h>

#include "CSSRuleImpl.h"
#include "MediaListImpl.h"
#include "CSSStyleSheetImpl.h"

using namespace KDOM;

MediaListImpl::MediaListImpl() : StyleBaseImpl(0)
{
}

MediaListImpl::MediaListImpl(CSSStyleSheetImpl *parentSheet, const DOMString &media) : StyleBaseImpl(parentSheet)
{
	setMediaText(media);
}

MediaListImpl::MediaListImpl(CSSRuleImpl *parentRule, const DOMString &media) : StyleBaseImpl(parentRule)
{
	setMediaText(media);
}

MediaListImpl::~MediaListImpl()
{
}

CSSRuleImpl *MediaListImpl::parentRule() const
{
	if(m_parent && m_parent->isRule())
		return static_cast<CSSRuleImpl *>(m_parent);

	return 0;
}

CSSStyleSheetImpl *MediaListImpl::parentStyleSheet() const
{
	if(m_parent && m_parent->isCSSStyleSheet())
		return static_cast<CSSStyleSheetImpl *>(m_parent);

	return 0;
}

void MediaListImpl::setMediaText(const DOMString &mediaText)
{
	m_lstMedia.clear();
	
	const QString val = mediaText.string();
	const QStringList list = QStringList::split(',', val);

	QStringList::ConstIterator it = list.begin();
	const QStringList::ConstIterator end = list.end();

	for(; it != end; ++it)
	{
		const DOMString medium = (*it).stripWhiteSpace();
		if(!medium.isEmpty())
			m_lstMedia.append(medium);
	}
}

DOMString MediaListImpl::mediaText() const
{
	DOMString text;
	
	QValueList<DOMString>::ConstIterator it = m_lstMedia.begin();
	const QValueList<DOMString>::ConstIterator end = m_lstMedia.end();
	
	for(; it != end; ++it)
	{
		text += *it;
		text += ", ";
	}

	return text;
}

unsigned long MediaListImpl::length() const
{
	return m_lstMedia.count();
}

DOMString MediaListImpl::item(unsigned long index) const
{
	return m_lstMedia[index];
}

void MediaListImpl::deleteMedium(const DOMString &oldMedium)
{
	QValueList<DOMString>::Iterator it = m_lstMedia.begin();
	const QValueList<DOMString>::Iterator end = m_lstMedia.end();

	for(; it != end; ++it)
	{
		if((*it) == oldMedium)
		{
			m_lstMedia.remove(it);
			return;
		}
	}
}

void MediaListImpl::appendMedium(const DOMString &newMedium)
{
	m_lstMedia.append(newMedium);
}

bool MediaListImpl::contains(const DOMString &medium) const
{
	return m_lstMedia.isEmpty() || m_lstMedia.contains(medium) || m_lstMedia.contains("all");
}

// vim:ts=4:noet
