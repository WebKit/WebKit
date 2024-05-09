/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WPEQtProfile.h"

#include "WPEQtProfilePrivate.h"
#include "WPEQtView.h"

/*!
  \qmltype WPEProfile
  \instantiates WPEQtProfile
  \inqmlmodule org.wpewebkit.qtwpe

  \brief A utility class resembling QtWebEngineProfile.
*/
WPEQtProfile::WPEQtProfile(const QString& httpUserAgent, const QString& httpAcceptLanguage)
    : d_ptr(new WPEQtProfilePrivate(httpUserAgent, httpAcceptLanguage))
{
}

WPEQtProfile::~WPEQtProfile()
{
}

/*!
    \qmlproperty string WPEProfile::httpUserAgent

    The user-agent string sent with HTTP to identify the browser.

    \note On Windows 8.1 and newer, the default user agent will always report
    "Windows NT 6.2" (Windows 8), unless the application does contain a manifest
    that declares newer Windows versions as supported.
*/

/*!
    \property WPEQtProfile::httpUserAgent

    The user-agent string sent with HTTP to identify the browser.
*/

QString WPEQtProfile::httpUserAgent() const
{
    const Q_D(WPEQtProfile);
    return d->m_httpUserAgent;
}

void WPEQtProfile::setHttpUserAgent(const QString& userAgent)
{
    Q_D(WPEQtProfile);
    if (d->m_httpUserAgent == userAgent)
        return;
    d->m_httpUserAgent = userAgent;
    Q_EMIT httpUserAgentChanged();
}

/*!
    \qmlproperty string WPEProfile::httpAcceptLanguage

    The value of the Accept-Language HTTP request-header field.
*/

/*!
    \property WPEQtProfile::httpAcceptLanguage

    The value of the Accept-Language HTTP request-header field.
*/

QString WPEQtProfile::httpAcceptLanguage() const
{
    Q_D(const WPEQtProfile);
    return d->m_httpAcceptLanguage;
}

void WPEQtProfile::setHttpAcceptLanguage(const QString& acceptLanguage)
{
    Q_D(WPEQtProfile);
    if (d->m_httpAcceptLanguage == acceptLanguage)
        return;
    d->m_httpAcceptLanguage = acceptLanguage;
    Q_EMIT httpAcceptLanguageChanged();
}

#include "moc_WPEQtProfile.cpp"
