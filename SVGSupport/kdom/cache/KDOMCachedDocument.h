/*
    Copyright (C) 2005 Frans Englich 		<frans.englich@telia.com>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc

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

#ifndef KDOM_CachedDocument_H
#define KDOM_CachedDocument_H

#include <kdom/cache/KDOMCachedObject.h>

class QBuffer;

namespace KDOM
{
	class DOMString;
	class Parser;

	class CachedDocument: public CachedObject
	{
	public:
		CachedDocument(DocumentLoader *docLoader, const DOMString &url, 
						KIO::CacheControl cachePolicy, const char *accept);

		virtual ~CachedDocument();

		virtual void ref(CachedObjectClient *consumer);

		virtual void data( QBuffer &buffer, bool eof );

		virtual void error( int err, const char *text );

		virtual bool schedule() const;

		const DOMString& document() const;

		void setCharset(const QString &charset);

		QBuffer* documentBuffer() const;

	protected:

		/**
		 * Calls all listerners, the object clients, of this cache object.
		 */
		void checkNotify();

	private:

		class Private;
		Private * d;
	};
};

#endif

// vim:ts=4:noet
