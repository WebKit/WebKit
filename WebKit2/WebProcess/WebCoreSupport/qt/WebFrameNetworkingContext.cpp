/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#include "WebFrameNetworkingContext.h"

#include <QNetworkAccessManager>
#include <QObject>

namespace WebCore {

WebFrameNetworkingContext::WebFrameNetworkingContext(Frame* frame)
    : FrameNetworkingContext(frame)
    , m_originatingObject(0)
    , m_networkAccessManager(new QNetworkAccessManager)
{
}

PassRefPtr<WebFrameNetworkingContext> WebFrameNetworkingContext::create(Frame* frame)
{
    return adoptRef(new WebFrameNetworkingContext(frame));
}

QObject* WebFrameNetworkingContext::originatingObject() const
{
    return m_originatingObject;
}

QNetworkAccessManager* WebFrameNetworkingContext::networkAccessManager() const
{
    return m_networkAccessManager;
}

}
