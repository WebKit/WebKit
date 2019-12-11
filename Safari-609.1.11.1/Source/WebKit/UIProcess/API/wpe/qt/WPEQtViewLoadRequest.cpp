/*
 * Copyright (C) 2018, 2019 Igalia S.L
 * Copyright (C) 2018, 2019 Zodiac Inflight Innovations
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
#include "WPEQtViewLoadRequest.h"

#include "WPEQtView.h"
#include "WPEQtViewLoadRequestPrivate.h"

/*!
  \qmltype WPEViewLoadRequest
  \instantiates WPEQtViewLoadRequest
  \inqmlmodule org.wpewebkit.qtwpe

  \brief A utility type for \l {WPEView}'s \l {WPEView::}{loadingChanged()} signal.

  The WPEViewLoadRequest type contains load status information for the requested URL.

  \sa {WPEView::loadingChanged()}{WPEView.loadingChanged()}
*/
WPEQtViewLoadRequest::WPEQtViewLoadRequest(const WPEQtViewLoadRequestPrivate& d)
    : d_ptr(new WPEQtViewLoadRequestPrivate(d))
{

}

WPEQtViewLoadRequest::~WPEQtViewLoadRequest()
{

}

/*!
  \qmlproperty url WPEView::WPEViewLoadRequest::url
  \readonly

  The URL of the load request.
*/
QUrl WPEQtViewLoadRequest::url() const
{
    Q_D(const WPEQtViewLoadRequest);
    return d->m_url;
}

/*!
  \qmlproperty enumeration WPEViewLoadRequest::status
  \readonly

  This enumeration represents the load status of a web page load request.

  \value WPEView.LoadStartedStatus The page is currently loading.
  \value WPEView.LoadStoppedStatus The page loading was interrupted.
  \value WPEView.LoadSucceededStatus The page was loaded successfully.
  \value WPEView.LoadFailedStatus The page could not be loaded.

  \sa {WPEView::loadingChanged()}{WPEView.loadingChanged}
*/
WPEQtView::LoadStatus WPEQtViewLoadRequest::status() const
{
    Q_D(const WPEQtViewLoadRequest);
    return d->m_status;
}

/*!
  \qmlproperty string WPEView::WPEViewLoadRequest::errorString
  \readonly

  Holds the error message if the load request failed.
*/
QString WPEQtViewLoadRequest::errorString() const
{
    Q_D(const WPEQtViewLoadRequest);
    return d->m_errorString;
}
