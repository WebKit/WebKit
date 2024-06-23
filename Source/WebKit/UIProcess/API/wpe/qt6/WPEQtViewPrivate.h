/*
 * Copyright (C) 2024 Igalia S.L
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

#pragma once

#include <QSize>
#include <QUrl>
#include <wpe/webkit.h>
#include <wtf/glib/GRefPtr.h>

class WPEQtViewPrivate {
public:
    WPEQtViewPrivate() { }
    ~WPEQtViewPrivate() { }

    GRefPtr<WebKitWebView> m_webView;
    QUrl m_url;
    QString m_html;
    QUrl m_baseUrl;
    QSize m_size { 800, 600 };
    bool m_errorOccured { false };
};

Q_DECLARE_METATYPE(WPEQtViewPrivate)
