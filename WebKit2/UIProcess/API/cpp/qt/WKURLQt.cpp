/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "WKURLQt.h"

#include "WKAPICast.h"
#include <QString>
#include <WebCore/PlatformString.h>
#include <wtf/RefPtr.h>

WKURLRef WKURLCreateWithQUrl(const QUrl& qURL)
{
    WebCore::String urlString(qURL.toString());
    RefPtr<WebCore::StringImpl> urlStringImpl = urlString.impl();
    return toURLRef(urlStringImpl.release().releaseRef());
}

QUrl WKURLCopyQUrl(WKURLRef urlRef)
{
    if (!urlRef)
        return QUrl();
    return QUrl(QString(reinterpret_cast<const QChar*>(toWK(urlRef)->characters()), toWK(urlRef)->length()));
}
