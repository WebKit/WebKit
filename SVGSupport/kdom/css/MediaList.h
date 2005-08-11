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

#ifndef KDOM_MediaList_H
#define KDOM_MediaList_H

#include <kdom/ecma/DOMLookup.h>

namespace KDOM
{
	class DOMString;
	class MediaListImpl;
	class MediaList 
	{
	public:
		MediaList();
		explicit MediaList(MediaListImpl *i);
		MediaList(const MediaList &other);
		virtual ~MediaList();

		// Operators
		MediaList &operator=(const MediaList &other);
		bool operator==(const MediaList &other) const;
		bool operator!=(const MediaList &other) const;

		// 'MediaList' functions
		void setMediaText(const DOMString &mediaText);
		DOMString mediaText() const;

		unsigned long length() const;
		DOMString item(unsigned long index) const;
		void deleteMedium(const DOMString &oldMedium);
		void appendMedium(const DOMString &newMedium);

		// Internal
		KDOM_INTERNAL_BASE(MediaList)
		
	protected:
		MediaListImpl *d;

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);		
	};
};

KDOM_DEFINE_PROTOTYPE(MediaListProto)
KDOM_IMPLEMENT_PROTOFUNC(MediaListProtoFunc, MediaList)

#endif

// vim:ts=4:noet
