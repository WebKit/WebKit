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
#include "WPEQmlExtensionPlugin.h"

#include "WPEQtView.h"
#include "WPEQtViewLoadRequest.h"
#include <qqml.h>

void WPEQmlExtensionPlugin::registerTypes(const char* uri)
{
    // @uri org.wpewebkit.qtwpe
    qmlRegisterType<WPEQtView>(uri, 1, 0, "WPEView");

    const QString& msg = QObject::tr("Cannot create separate instance of WPEQtViewLoadRequest");
    qmlRegisterUncreatableType<WPEQtViewLoadRequest>(uri, 1, 0, "WPEViewLoadRequest", msg);
}
