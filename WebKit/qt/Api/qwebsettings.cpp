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
        : minimumFontSize(-1),
          minimumLogicalFontSize(-1),
          defaultFontSize(-1),
          defaultFixedFontSize(-1),
          settings(wcSettings)
    {
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

Q_GLOBAL_STATIC(QList<QWebSettingsPrivate *>, allSettings);

void QWebSettingsPrivate::apply()
{
    if (settings) {
        QWebSettingsPrivate *global = QWebSettings::defaultSettings()->d;

        QString family = fontFamilies.value(QWebSettings::StandardFont,
                                            global->fontFamilies.value(QWebSettings::StandardFont));
        settings->setStandardFontFamily(family);

        family = fontFamilies.value(QWebSettings::FixedFont,
                                    global->fontFamilies.value(QWebSettings::FixedFont));
        settings->setFixedFontFamily(family);

        family = fontFamilies.value(QWebSettings::SerifFont,
                                    global->fontFamilies.value(QWebSettings::SerifFont));
        settings->setSerifFontFamily(family);

        family = fontFamilies.value(QWebSettings::SansSerifFont,
                                    global->fontFamilies.value(QWebSettings::SansSerifFont));
        settings->setSansSerifFontFamily(family);

        family = fontFamilies.value(QWebSettings::CursiveFont,
                                    global->fontFamilies.value(QWebSettings::CursiveFont));
        settings->setCursiveFontFamily(family);

        family = fontFamilies.value(QWebSettings::FantasyFont,
                                    global->fontFamilies.value(QWebSettings::FantasyFont));
        settings->setFantasyFontFamily(family);

        int size = minimumFontSize >= 0 ? minimumFontSize : global->minimumFontSize;
        settings->setMinimumFontSize(size);

        size = minimumLogicalFontSize >= 0 ? minimumLogicalFontSize : global->minimumLogicalFontSize;
        settings->setMinimumLogicalFontSize(size);

        size = defaultFontSize >= 0 ? defaultFontSize : global->defaultFontSize;
        settings->setDefaultFontSize(size);

        size = defaultFixedFontSize >= 0 ? defaultFixedFontSize : global->defaultFixedFontSize;
        settings->setDefaultFixedFontSize(size);

        bool value = attributes.value(QWebSettings::AutoLoadImages,
                                      global->attributes.value(QWebSettings::AutoLoadImages));
        settings->setLoadsImagesAutomatically(value);

        value = attributes.value(QWebSettings::JavascriptEnabled,
                                      global->attributes.value(QWebSettings::JavascriptEnabled));
        settings->setJavaScriptEnabled(value);

        value = attributes.value(QWebSettings::JavascriptCanOpenWindows,
                                      global->attributes.value(QWebSettings::JavascriptCanOpenWindows));
        settings->setJavaScriptCanOpenWindowsAutomatically(value);

        value = attributes.value(QWebSettings::JavaEnabled,
                                      global->attributes.value(QWebSettings::JavaEnabled));
        settings->setJavaEnabled(value);

        value = attributes.value(QWebSettings::PluginsEnabled,
                                      global->attributes.value(QWebSettings::PluginsEnabled));
        settings->setPluginsEnabled(value);

        value = attributes.value(QWebSettings::PrivateBrowsingEnabled,
                                      global->attributes.value(QWebSettings::PrivateBrowsingEnabled));
        settings->setPrivateBrowsingEnabled(value);

        QString location = (!userStyleSheetLocation.isEmpty()) ? userStyleSheetLocation : global->userStyleSheetLocation;
        settings->setUserStyleSheetLocation(WebCore::KURL(location));
    } else {
        QList<QWebSettingsPrivate *> settings = *::allSettings();
        for (int i = 0; i < settings.count(); ++i)
            settings[i]->apply();
    }
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
    // Initialize our global defaults
    // changing any of those will likely break the LayoutTests
    d->minimumFontSize = 5;
    d->minimumLogicalFontSize = 5;
    d->defaultFontSize = 14;
    d->defaultFixedFontSize = 14;
    d->fontFamilies.insert(QWebSettings::StandardFont, QLatin1String("Arial"));
    d->fontFamilies.insert(QWebSettings::StandardFont, QLatin1String("Arial"));
    d->fontFamilies.insert(QWebSettings::FixedFont, QLatin1String("Courier"));
    d->fontFamilies.insert(QWebSettings::SerifFont, QLatin1String("Times New Roman"));
    d->fontFamilies.insert(QWebSettings::SansSerifFont, QLatin1String("Arial"));

    d->attributes.insert(QWebSettings::AutoLoadImages, true);
    d->attributes.insert(QWebSettings::JavascriptEnabled, true);
}

QWebSettings::QWebSettings(WebCore::Settings *settings)
    : d(new QWebSettingsPrivate(settings))
{
    d->settings = settings;
    d->apply();
    allSettings()->append(d);
}

QWebSettings::~QWebSettings()
{
    if (d->settings)
        allSettings()->removeOne(d);
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
    if (family.isEmpty())
        d->fontFamilies.remove(type);
    else
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

void QWebSettings::clearAttribute(WebAttribute attr)
{
    d->attributes.remove(attr);
    d->apply();
}

