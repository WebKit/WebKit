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

#ifndef KDOM_CachedObject_H
#define KDOM_CachedObject_H

#include <qbuffer.h>
#include <qptrdict.h>
#include <qtextcodec.h>

#include <kio/global.h>

#include <kdom/DOMString.h>

#include <time.h>

namespace KDOM
{
    class DOMString;
    class CachedObject;
    class DocumentLoader;
    class CachedObjectClient;
   
    class Request
    {
    public:
        Request(DocumentLoader *_docLoader, CachedObject *_object, bool _incremental);
        ~Request();

        bool incremental;
        QBuffer buffer;
        CachedObject *object;
        DocumentLoader *docLoader;
    };

    class CachedObject
    {
    public:
        enum Type
        {
            Image,
            StyleSheet,
            Script,

            /**
             * A Text document. Such as an XML file or plain text.
             */
            TextDocument
        };

        enum Status
        {
            Unknown,
            New,
            Pending,
            Persistent,
            Cached
        };

        CachedObject(const DOMString &url, Type type, KIO::CacheControl cachePolicy, int size);
          virtual ~CachedObject();

        virtual void data(QBuffer &buffer, bool eof) = 0;
        virtual void error(int err, const char *text) = 0;

        virtual void ref(CachedObjectClient *consumer);
        virtual void deref(CachedObjectClient *consumer);

        virtual bool schedule() const;
        virtual void finish();
    
        QTextCodec *codecForBuffer(const QString &charset, const QByteArray &buffer) const;

        void setRequest(Request *request);

        bool isExpired() const;

        // Helpers
        const DOMString &url() const { return m_url; }
        Type type() const { return m_type; }

        int count() const { return m_clients.count(); }
        int accessCount() const { return m_accessCount; }

        void setStatus(Status s) { m_status = s; }
        Status status() const { return m_status; }

        int size() const { return m_size; }
        bool free() const { return m_free; }

        KIO::CacheControl cachePolicy() const { return m_cachePolicy; }

        bool canDelete() const { return (m_clients.count() == 0 && !m_request); }
        void setExpireDate(time_t _expireDate) { m_expireDate = _expireDate; }

        /**
         * List of acceptable mimetypes separated by ",". A mimetype may contain a wildcard.
         */
        QString accept() const { return m_accept; }
        void setAccept(const QString &accept) { m_accept = accept; }

        /**
         * Sets the language-accept HTTP header field.
         */
        QString acceptLanguage() const { return m_acceptLanguage; }
        void setAcceptLanguage(const QString &lang) { m_acceptLanguage = lang; }

    protected:
        void setSize(int size);

        QPtrDict<CachedObjectClient> m_clients;

        int m_size;
        Type m_type;
        Status m_status;
        DOMString m_url;
        QString m_acceptLanguage;
        QString m_accept;
        int m_accessCount;
        Request *m_request;
        time_t m_expireDate;
        KIO::CacheControl m_cachePolicy;

        bool m_free : 1;
        bool m_deleted : 1;
        bool m_loading : 1;
        bool m_hadError : 1;
        bool m_wasBlocked : 1;

    private:
        friend class Cache;

        bool allowInLRUList() const { return canDelete() && !m_free && status() != Persistent; }

        CachedObject *m_next;
        CachedObject *m_prev;
    };
};

#endif

// vim:ts=4:noet
