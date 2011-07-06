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
#include "qwkpreferences.h"

#include "WKPageGroup.h"
#include "WKPreferences.h"
#include "WKStringQt.h"
#include "WKRetainPtr.h"
#include "qwkpreferences_p.h"


QWKPreferences* QWKPreferencesPrivate::createPreferences(WKPageGroupRef pageGroupRef)
{
    QWKPreferences* prefs = new QWKPreferences;
    prefs->d->ref = WKPageGroupGetPreferences(pageGroupRef);
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

void QWKPreferences::setFontFamily(FontFamily which, const QString& family)
{
    switch (which) {
    case StandardFont:
        WKPreferencesSetStandardFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    case FixedFont:
        WKPreferencesSetFixedFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    case SerifFont:
        WKPreferencesSetSerifFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    case SansSerifFont:
        WKPreferencesSetSansSerifFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    case CursiveFont:
        WKPreferencesSetCursiveFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    case FantasyFont:
        WKPreferencesSetFantasyFontFamily(d->ref, WKStringCreateWithQString(family));
        break;
    default:
        break;
    }
}

QString QWKPreferences::fontFamily(FontFamily which) const
{
    switch (which) {
    case StandardFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopyStandardFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    case FixedFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopyFixedFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    case SerifFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopySerifFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    case SansSerifFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopySansSerifFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    case CursiveFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopyCursiveFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    case FantasyFont: {
        WKRetainPtr<WKStringRef> stringRef(AdoptWK, WKPreferencesCopyFantasyFontFamily(d->ref));
        return WKStringCopyQString(stringRef.get());
    }
    default:
        return QString();
    }
}

bool QWKPreferences::testAttribute(WebAttribute attr) const
{
    switch (attr) {
    case AutoLoadImages:
        return WKPreferencesGetLoadsImagesAutomatically(d->ref);
    case JavascriptEnabled:
        return WKPreferencesGetJavaScriptEnabled(d->ref);
    case PluginsEnabled:
        return WKPreferencesGetPluginsEnabled(d->ref);
    case OfflineWebApplicationCacheEnabled:
        return WKPreferencesGetOfflineWebApplicationCacheEnabled(d->ref);
    case LocalStorageEnabled:
        return WKPreferencesGetLocalStorageEnabled(d->ref);
    case XSSAuditingEnabled:
        return WKPreferencesGetXSSAuditorEnabled(d->ref);
    case FrameFlatteningEnabled:
        return WKPreferencesGetFrameFlatteningEnabled(d->ref);
    case PrivateBrowsingEnabled:
        return WKPreferencesGetPrivateBrowsingEnabled(d->ref);
    case DeveloperExtrasEnabled:
        return WKPreferencesGetDeveloperExtrasEnabled(d->ref);
    case DnsPrefetchEnabled:
        return WKPreferencesGetDNSPrefetchingEnabled(d->ref);
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

void QWKPreferences::setAttribute(WebAttribute attr, bool on)
{
    switch (attr) {
    case AutoLoadImages:
        WKPreferencesSetLoadsImagesAutomatically(d->ref, on);
        break;
    case JavascriptEnabled:
        WKPreferencesSetJavaScriptEnabled(d->ref, on);
        break;
    case PluginsEnabled:
        WKPreferencesSetPluginsEnabled(d->ref, on);
        break;
    case OfflineWebApplicationCacheEnabled:
        WKPreferencesSetOfflineWebApplicationCacheEnabled(d->ref, on);
        break;
    case LocalStorageEnabled:
        WKPreferencesSetLocalStorageEnabled(d->ref, on);
        break;
    case XSSAuditingEnabled:
        WKPreferencesSetXSSAuditorEnabled(d->ref, on);
        break;
    case FrameFlatteningEnabled:
        WKPreferencesSetFrameFlatteningEnabled(d->ref, on);
        break;
    case PrivateBrowsingEnabled:
        WKPreferencesSetPrivateBrowsingEnabled(d->ref, on);
        break;
    case DeveloperExtrasEnabled:
        WKPreferencesSetDeveloperExtrasEnabled(d->ref, on);
        break;
    case DnsPrefetchEnabled:
        WKPreferencesSetDNSPrefetchingEnabled(d->ref, on);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void QWKPreferences::setFontSize(FontSize type, int size)
{
    switch (type) {
    case MinimumFontSize:
         WKPreferencesSetMinimumFontSize(d->ref, size);
         break;
    case DefaultFontSize:
         WKPreferencesSetDefaultFontSize(d->ref, size);
         break;
    case DefaultFixedFontSize:
         WKPreferencesSetDefaultFixedFontSize(d->ref, size);
         break;
    default:
        ASSERT_NOT_REACHED();
    }
}

int QWKPreferences::fontSize(FontSize type) const
{
    switch (type) {
    case MinimumFontSize:
         return WKPreferencesGetMinimumFontSize(d->ref);
    case DefaultFontSize:
         return WKPreferencesGetDefaultFontSize(d->ref);
    case DefaultFixedFontSize:
         return WKPreferencesGetDefaultFixedFontSize(d->ref);
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

