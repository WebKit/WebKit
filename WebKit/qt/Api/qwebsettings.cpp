/*
    Copyright (C) 2007 Trolltech ASA

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "qwebsettings.h"

#include "qwebpage.h"
#include "qwebpage_p.h"

#include "Page.h"
#include "Settings.h"
#include "KURL.h"
#include "PlatformString.h"
#include "IconDatabase.h"

#include <QHash>
#include <QSharedData>

class QWebSettingsPrivate
{
public:
    QWebSettingsPrivate(WebCore::Settings *wcSettings = 0)
        : minimumFontSize(5),
          minimumLogicalFontSize(5),
          defaultFontSize(14),
          defaultFixedFontSize(14),
          settings(wcSettings)
    {
        // Initialize our defaults
        // changing any of those will likely break the LayoutTests
        fontFamilies.insert(QWebSettings::StandardFont, QLatin1String("Arial"));
        fontFamilies.insert(QWebSettings::StandardFont, QLatin1String("Arial"));
        fontFamilies.insert(QWebSettings::FixedFont, QLatin1String("Courier"));
        fontFamilies.insert(QWebSettings::SerifFont, QLatin1String("Times New Roman"));
        fontFamilies.insert(QWebSettings::SansSerifFont, QLatin1String("Arial"));

        attributes.insert(QWebSettings::AutoLoadImages, true);
        attributes.insert(QWebSettings::JavascriptEnabled, true);
    }

    QHash<int, QString> fontFamilies;
    int minimumFontSize;
    int minimumLogicalFontSize;
    int defaultFontSize;
    int defaultFixedFontSize;
    QHash<int, bool> attributes;
    QString userStyleSheetLocation;

    void apply();
    WebCore::Settings *settings;
};

typedef QHash<int, QPixmap> WebGraphicHash;
Q_GLOBAL_STATIC(WebGraphicHash, graphics)

void QWebSettingsPrivate::apply()
{
    if (!settings)
        return;

    settings->setStandardFontFamily(fontFamilies.value(QWebSettings::StandardFont));
    settings->setFixedFontFamily(fontFamilies.value(QWebSettings::FixedFont));
    settings->setSerifFontFamily(fontFamilies.value(QWebSettings::SerifFont));
    settings->setSansSerifFontFamily(fontFamilies.value(QWebSettings::SansSerifFont));
    settings->setCursiveFontFamily(fontFamilies.value(QWebSettings::CursiveFont));
    settings->setFantasyFontFamily(fontFamilies.value(QWebSettings::FantasyFont));

    settings->setMinimumFontSize(minimumFontSize);
    settings->setMinimumLogicalFontSize(minimumLogicalFontSize);
    settings->setDefaultFontSize(defaultFontSize);
    settings->setDefaultFixedFontSize(defaultFixedFontSize);

    settings->setLoadsImagesAutomatically(attributes.value(QWebSettings::AutoLoadImages));
    settings->setJavaScriptEnabled(attributes.value(QWebSettings::JavascriptEnabled));
    settings->setJavaScriptCanOpenWindowsAutomatically(attributes.value(QWebSettings::JavascriptCanOpenWindows));
    settings->setJavaEnabled(attributes.value(QWebSettings::JavaEnabled));
    settings->setPluginsEnabled(attributes.value(QWebSettings::PluginsEnabled));
    settings->setPrivateBrowsingEnabled(attributes.value(QWebSettings::PrivateBrowsingEnabled));

    settings->setUserStyleSheetLocation(WebCore::KURL(userStyleSheetLocation));
}

QWebSettings *QWebSettings::defaultSettings()
{
    static QWebSettings *global = 0;
    if (!global)
        global = new QWebSettings;
    return global;
}

QWebSettings::QWebSettings()
    : d(new QWebSettingsPrivate)
{
}

QWebSettings::QWebSettings(WebCore::Settings *settings)
    : d(new QWebSettingsPrivate(settings))
{
    // inherit the global default settings
    *d = *defaultSettings()->d;
    d->settings = settings;
    d->apply();
}

QWebSettings::~QWebSettings()
{
}

void QWebSettings::setMinimumFontSize(int size)
{
    d->minimumFontSize = size;
    d->apply();
}


int QWebSettings::minimumFontSize() const
{
    return d->minimumFontSize;
}


void QWebSettings::setMinimumLogicalFontSize(int size)
{
    d->minimumLogicalFontSize = size;
    d->apply();
}


int QWebSettings::minimumLogicalFontSize() const
{
    return d->minimumLogicalFontSize;
}


void QWebSettings::setDefaultFontSize(int size)
{
    d->defaultFontSize = size;
    d->apply();
}


int QWebSettings::defaultFontSize() const
{
    return d->defaultFontSize;
}


void QWebSettings::setDefaultFixedFontSize(int size)
{
    d->defaultFixedFontSize = size;
    d->apply();
}


int QWebSettings::defaultFixedFontSize() const
{
    return d->defaultFixedFontSize;
}

void QWebSettings::setUserStyleSheetLocation(const QString &location)
{
    d->userStyleSheetLocation = location;
    d->apply();
}

QString QWebSettings::userStyleSheetLocation() const
{
    return d->userStyleSheetLocation;
}

void QWebSettings::setIconDatabaseEnabled(bool enabled, const QString &location)
{
    WebCore::iconDatabase()->setEnabled(enabled);
    if (enabled) {
      if (!location.isEmpty()) {
          WebCore::iconDatabase()->open(location);
      } else {
          WebCore::iconDatabase()->open(WebCore::iconDatabase()->defaultDatabaseFilename());
      }
    } else {
      WebCore::iconDatabase()->close();
    }
}

bool QWebSettings::iconDatabaseEnabled()
{
    return WebCore::iconDatabase()->isEnabled() && WebCore::iconDatabase()->isOpen();
}

void QWebSettings::setWebGraphic(WebGraphic type, const QPixmap &graphic)
{
    WebGraphicHash *h = graphics();
    if (graphic.isNull())
        h->remove(type);
    else
        h->insert(type, graphic);
}

QPixmap QWebSettings::webGraphic(WebGraphic type)
{
    return graphics()->value(type);
}

void QWebSettings::setFontFamily(FontType type, const QString &family)
{
    d->fontFamilies.insert(type, family);
    d->apply();
}

QString QWebSettings::fontFamily(FontType type) const
{
    return d->fontFamilies.value(type);
}

void QWebSettings::setAttribute(WebAttribute attr, bool on)
{
    d->attributes.insert(attr, on);
    d->apply();
}

bool QWebSettings::testAttribute(WebAttribute attr) const
{
    return d->attributes.value(attr);
}

