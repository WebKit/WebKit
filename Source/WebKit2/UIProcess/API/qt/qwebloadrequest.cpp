/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include "qwebloadrequest_p.h"

class QWebLoadRequestPrivate {
public:
    QWebLoadRequestPrivate(const QUrl& url, QQuickWebView::LoadStatus status, const QString& errorString, QQuickWebView::ErrorDomain errorDomain, int errorCode)
        : url(url)
        , status(status)
        , errorString(errorString)
        , errorDomain(errorDomain)
        , errorCode(errorCode)
    {
    }

    QUrl url;
    QQuickWebView::LoadStatus status;
    QString errorString;
    QQuickWebView::ErrorDomain errorDomain;
    int errorCode;
};

QWebLoadRequest::QWebLoadRequest(const QUrl& url, QQuickWebView::LoadStatus status, const QString& errorString, QQuickWebView::ErrorDomain errorDomain, int errorCode, QObject* parent)
    : QObject(parent)
    , d(new QWebLoadRequestPrivate(url, status, errorString, errorDomain, errorCode))
{
}

QWebLoadRequest::~QWebLoadRequest()
{
}

QUrl QWebLoadRequest::url() const
{
    return d->url;
}

QQuickWebView::LoadStatus QWebLoadRequest::status() const
{
    return d->status;
}

QString QWebLoadRequest::errorString() const
{
    return d->errorString;
}

QQuickWebView::ErrorDomain QWebLoadRequest::errorDomain() const
{
    return d->errorDomain;
}

int QWebLoadRequest::errorCode() const
{
    return d->errorCode;
}
