/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KDOM_Loader_H
#define KDOM_Loader_H

#include <qtimer.h>
#include <qobject.h>
#include <qptrdict.h>
#include <qstringlist.h>

#include <kio/job.h>
#include <kio/global.h>

#include <kdom/cache/KDOMCachedObject.h>

namespace KDOM
{
	class Request;
	class KDOMPart;
	class DOMString;
	class CachedImage;
	class CachedObject;
	class CachedScript;
	class DocumentImpl;
	class CachedDocument;
	class CachedStyleSheet;

	// TODO Move to KDOMSettings class!
	enum KAnimationAdvice
	{
		KAnimationDisabled = 0,
		KAnimationLoopOnce,
		KAnimationEnabled
	};
	
	class DocumentLoader
	{
	public:
		DocumentLoader(KDOMPart *part, DocumentImpl *docImpl);
	 	~DocumentLoader();

		CachedImage *requestImage(const KURL &url);
		CachedStyleSheet *requestStyleSheet(const KURL &url, const QString &charset,
											const char *accept = "text/css", 
											bool userSheet = false);

		CachedScript *requestScript(const KURL &url, const QString &charset);
		CachedDocument *requestDocument(const KURL &url, const QString &charset,
										const char *accept = 0);

		KAnimationAdvice showAnimations() const { return m_showAnimations; }
		bool autoloadImages() const { return m_autoloadImages; }
		KIO::CacheControl cachePolicy() const { return m_cachePolicy; }
		time_t expireDate() const { return m_expireDate; }
		DocumentImpl *doc() const { return m_doc; }

		void setCacheCreationDate(time_t date);
		void setExpireDate(time_t date, bool relative);
		void setAutoloadImages(bool load);
		void setShowAnimations(KAnimationAdvice showAnimations);
		void setCachePolicy(KIO::CacheControl cachePolicy) { m_cachePolicy = cachePolicy; }

		void insertCachedObject(CachedObject *object) const;
		void removeCachedObject(CachedObject *object) const;
	
		bool needReload(const KURL &fullUrl);

		bool hasPending(CachedObject::Type type) const;

		/**
		 * Sets the referrer header field. This can be used when using the DocumentLoader
		 * without a DocumentImpl.
		 */
		void setReferrer(const KURL& uri);

		/**
		 * When a @ref DocumentImpl have been associated with the DocumentLoader, its document URI
		 * is returned. Otherwise, any URI set via setReferrer() is returned.
		 *
		 * @returns the originator
		 */
		KURL referrer() const;

	private:
		KURL uri;

		KDOMPart *m_part;
		DocumentImpl *m_doc;

		bool m_autoloadImages : 1;
		KAnimationAdvice m_showAnimations : 2;

		time_t m_expireDate;
		time_t m_creationDate;

		QStringList m_reloadedURLs;

		KIO::CacheControl m_cachePolicy;
		mutable QPtrDict<CachedObject> m_docObjects;
	};

	class Loader : public QObject
	{
	Q_OBJECT
	public:
		Loader();

		void load(DocumentLoader *docLoader, CachedObject *object, bool incremental = true);

		int numRequests(DocumentLoader *docLoader) const;
		void cancelRequests(DocumentLoader *docLoader);

		KIO::Job *jobForRequest(const DOMString &url) const;
		
#ifdef APPLE_CHANGES
		KWQSignal _requestStarted;
		KWQSignal _requestDone;
		KWQSignal _requestFailed;
#endif

	signals:
		void requestStarted(DocumentLoader *docLoader, CachedObject *obj);
		void requestDone(DocumentLoader *docLoader, CachedObject *obj);
		void requestFailed(DocumentLoader *docLoader, CachedObject *obj);

	protected slots:
		void slotFinished(KIO::Job *job);
		void slotData(KIO::Job *job, const QByteArray &buffer);

		void servePendingRequests();

	protected:
		QPtrList<Request> m_requestsPending;
		QPtrDict<Request> m_requestsLoading;

		QTimer m_timer;
	};
};

#endif

// vim:ts=4:noet
