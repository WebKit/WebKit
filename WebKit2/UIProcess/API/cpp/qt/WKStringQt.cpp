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

#include "WKStringQt.h"

#include "WKAPICast.h"
#include <QString>
#include <WebCore/PlatformString.h>
#include <wtf/RefPtr.h>

WKStringRef WKStringCreateWithQString(const QString& qString)
{
    WebCore::String string(qString);
    RefPtr<WebCore::StringImpl> stringImpl = string.impl();
    return toRef(stringImpl.release().releaseRef());
}

QString WKStringCopyQString(WKStringRef stringRef)
{
    if (!stringRef)
        return QString();
    return QString(reinterpret_cast<const QChar*>(toWK(stringRef)->characters()), toWK(stringRef)->length());
}
