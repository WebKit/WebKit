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

#include "qwkpreferences.h"

#include "WKContext.h"
#include "WKPreferences.h"
#include "qwkpreferences_p.h"

QWKPreferences* QWKPreferencesPrivate::createPreferences(WKContextRef contextRef)
{
    QWKPreferences* prefs = new QWKPreferences;
    prefs->d->ref = WKContextGetPreferences(contextRef);
    return prefs;
}

QWKPreferences* QWKPreferencesPrivate::createSharedPreferences()
{
    QWKPreferences* prefs = new QWKPreferences;
    prefs->d->ref = WKPreferencesCreate();
    return prefs;
}

QWKPreferences* QWKPreferences::sharedPreferences()
{
    static QWKPreferences* instance = 0;

    if (!instance)
        instance = QWKPreferencesPrivate::createSharedPreferences();
    return instance;
}

QWKPreferences::QWKPreferences()
    : d(new QWKPreferencesPrivate)
{
}

QWKPreferences::~QWKPreferences()
{
    delete d;
}

bool QWKPreferences::testAttribute(WebAttribute attr) const
{
    switch (attr) {
    case AutoLoadImages:
        return WKPreferencesGetLoadsImagesAutomatically(d->ref);
    case JavascriptEnabled:
        return WKPreferencesGetJavaScriptEnabled(d->ref);
    case OfflineWebApplicationCacheEnabled:
        return WKPreferencesGetOfflineWebApplicationCacheEnabled(d->ref);
    case LocalStorageEnabled:
        return WKPreferencesGetLocalStorageEnabled(d->ref);
    case XSSAuditingEnabled:
        return WKPreferencesGetXSSAuditorEnabled(d->ref);
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

void QWKPreferences::setAttribute(WebAttribute attr, bool on)
{
    switch (attr) {
    case AutoLoadImages:
        return WKPreferencesSetLoadsImagesAutomatically(d->ref, on);
    case JavascriptEnabled:
        return WKPreferencesSetJavaScriptEnabled(d->ref, on);
    case OfflineWebApplicationCacheEnabled:
        return WKPreferencesSetOfflineWebApplicationCacheEnabled(d->ref, on);
    case LocalStorageEnabled:
        return WKPreferencesSetLocalStorageEnabled(d->ref, on);
    case XSSAuditingEnabled:
        return WKPreferencesSetXSSAuditorEnabled(d->ref, on);
    default:
        ASSERT_NOT_REACHED();
    }
}
