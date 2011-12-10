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

#include "qwebnavigationrequest_p.h"

#include "qquickwebview_p.h"

class QWebNavigationRequestPrivate {
public:
    QWebNavigationRequestPrivate(const QUrl& url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
        : url(url)
        , button(button)
        , modifiers(modifiers)
        , action(QQuickWebView::AcceptRequest)
    {
    }

    ~QWebNavigationRequestPrivate()
    {
    }

    QUrl url;
    Qt::MouseButton button;
    Qt::KeyboardModifiers modifiers;
    int action;
};

QWebNavigationRequest::QWebNavigationRequest(const QUrl& url, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, QObject* parent)
    : QObject(parent)
    , d(new QWebNavigationRequestPrivate(url, button, modifiers))
{
}

QWebNavigationRequest::~QWebNavigationRequest()
{
    delete d;
}

void QWebNavigationRequest::setAction(int action)
{
    if (d->action == action)
        return;

    d->action = action;
    emit actionChanged();
}

QUrl QWebNavigationRequest::url() const
{
    return d->url;
}

int QWebNavigationRequest::button() const
{
    return int(d->button);
}

int QWebNavigationRequest::modifiers() const
{
    return int(d->modifiers);
}

int QWebNavigationRequest::action() const
{
    return int(d->action);
}

