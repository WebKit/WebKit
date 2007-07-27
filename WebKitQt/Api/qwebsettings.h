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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef QWEBSETTINGS_H
#define QWEBSETTINGS_H

#include <qwebkitglobal.h>

#include <QString>
#include <QPixmap>
#include <QSharedDataPointer>

class QWebPage;
class QWebSettingsPrivate;

class QWEBKIT_EXPORT QWebSettings
{
public:
    static void setGlobal(const QWebSettings &settings);
    static QWebSettings global();
public:
    enum FontType {
        StandardFont,
        FixedFont,
        SerifFont,
        SansSerifFont,
        CursiveFont,
        FantasyFont
    };
    enum WebAttribute {
        AutoLoadImages,
        JavascriptEnabled,
        JavaEnabled,
        PluginsEnabled,
        PrivateBrowsingEnabled,
        JavascriptCanOpenWindows
    };
    enum WebGraphic {
        MissingImageGraphic,
        MissingPluginGraphic,
        DefaultFaviconGraphic,
        TextAreaResizeCornerGraphic
    };

    QWebSettings();
    ~QWebSettings();

    QWebSettings(const QWebSettings &);
    QWebSettings &operator=(const QWebSettings &);

    void setFontFamily(FontType type, const QString &family);
    QString fontFamily(FontType type) const;

    void setMinimumFontSize(int);
    int minimumFontSize() const;

    void setMinimumLogicalFontSize(int);
    int minimumLogicalFontSize() const;

    void setDefaultFontSize(int);
    int defaultFontSize() const;

    void setDefaultFixedFontSize(int);
    int defaultFixedFontSize() const;

    void setAttribute(WebAttribute attr, bool on = true);
    bool testAttribute(WebAttribute attr) const;

    void setUserStyleSheetLocation(const QString &location);
    QString userStyleSheetLocation() const;

    void setIconDatabaseEnabled(bool enabled, const QString &location = QString());
    bool iconDatabaseEnabled() const;

    void setWebGraphic(WebGraphic type, const QPixmap &graphic);
    QPixmap webGraphic(WebGraphic type) const;

private:
    QSharedDataPointer<QWebSettingsPrivate> d;
};

#endif
