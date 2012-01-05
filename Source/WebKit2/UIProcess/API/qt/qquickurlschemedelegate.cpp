/*
 * Copyright (C) 2011 Zeno Albisser <zeno@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qquickurlschemedelegate_p.h"

#include "qquicknetworkreply_p.h"
#include "qquicknetworkrequest_p.h"

QQuickUrlSchemeDelegate::QQuickUrlSchemeDelegate(QObject* parent)
    : QObject(parent)
    , m_request(new QQuickNetworkRequest(this))
    , m_reply(new QQuickNetworkReply(this))
{ }

QString QQuickUrlSchemeDelegate::scheme() const
{
    return m_scheme;
}

void QQuickUrlSchemeDelegate::setScheme(const QString& scheme)
{
    m_scheme = scheme;
    emit schemeChanged();
}

QQuickNetworkRequest* QQuickUrlSchemeDelegate::request() const
{
    return m_request;
}

QQuickNetworkReply* QQuickUrlSchemeDelegate::reply() const
{
    return m_reply;
}

#include "moc_qquickurlschemedelegate_p.cpp"
