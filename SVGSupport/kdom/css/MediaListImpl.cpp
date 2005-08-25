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

#include <qstringlist.h>

#include "CSSRuleImpl.h"
#include "MediaListImpl.h"
#include "DOMStringImpl.h"
#include "CSSStyleSheetImpl.h"

using namespace KDOM;

MediaListImpl::MediaListImpl() : StyleBaseImpl(0)
{
}

MediaListImpl::MediaListImpl(CSSStyleSheetImpl *parentSheet, DOMStringImpl *media) : StyleBaseImpl(parentSheet)
{
	setMediaText(media);
}

MediaListImpl::MediaListImpl(CSSRuleImpl *parentRule, DOMStringImpl *media) : StyleBaseImpl(parentRule)
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

DOMStringImpl *MediaListImpl::mediaText() const
{
	DOMStringImpl *text = new DOMStringImpl();
	
	QValueList<DOMString>::ConstIterator it = m_lstMedia.begin();
	const QValueList<DOMString>::ConstIterator end = m_lstMedia.end();
	
	for(; it != end; ++it)
		text->append((*it).string() + QString::fromLatin1(", "));

	return text;
}

void MediaListImpl::setMediaText(DOMStringImpl *mediaText)
{
	m_lstMedia.clear();
	
	const QString val = (mediaText ? mediaText->string() : QString::null);
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

unsigned long MediaListImpl::length() const
{
	return m_lstMedia.count();
}

DOMStringImpl *MediaListImpl::item(unsigned long index) const
{
	return (m_lstMedia[index]).handle();
}

void MediaListImpl::deleteMedium(DOMStringImpl *oldMedium)
{
	QValueList<DOMString>::Iterator it = m_lstMedia.begin();
	const QValueList<DOMString>::Iterator end = m_lstMedia.end();

	for(; it != end; ++it)
	{
		if((*it) == DOMString(oldMedium))
		{
			m_lstMedia.remove(it);
			return;
		}
	}
}

void MediaListImpl::appendMedium(DOMStringImpl *newMedium)
{
	m_lstMedia.append(DOMString(newMedium));
}

bool MediaListImpl::contains(DOMStringImpl *medium) const
{
	return m_lstMedia.isEmpty() ||
		   m_lstMedia.contains("all") ||
		   m_lstMedia.contains(DOMString(medium));
}

// vim:ts=4:noet
