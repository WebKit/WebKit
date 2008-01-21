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

#ifndef QWEBSETTINGS_H
#define QWEBSETTINGS_H

#include <qwebkitglobal.h>

#include <QtCore/qstring.h>
#include <QtGui/qpixmap.h>
#include <QtCore/qshareddata.h>

namespace WebCore
{
    class Settings;
};

class QWebPage;
class QWebSettingsPrivate;
class QUrl;

class QWEBKIT_EXPORT QWebSettings
{
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
        JavascriptCanOpenWindows,
        JavascriptCanAccessClipboard,
        DeveloperExtrasEnabled,
        LinksIncludedInFocusChain
    };
    enum WebGraphic {
        MissingImageGraphic,
        MissingPluginGraphic,
        DefaultFaviconGraphic,
        TextAreaResizeCornerGraphic
    };
    enum FontSize {
        MinimumFontSize,
        MinimumLogicalFontSize,
        DefaultFontSize,
        DefaultFixedFontSize
    };

    static QWebSettings *defaultSettings();

    void setFontFamily(FontType type, const QString &family);
    QString fontFamily(FontType type) const;
    void resetFontFamily(FontType type);

    void setFontSize(FontSize type, int size);
    int fontSize(FontSize type) const;
    void resetFontSize(FontSize type);

    void setAttribute(WebAttribute attr, bool on = true);
    bool testAttribute(WebAttribute attr) const;
    void clearAttribute(WebAttribute attr);

    void setUserStyleSheetLocation(const QUrl &location);
    QUrl userStyleSheetLocation() const;

    static void setIconDatabaseEnabled(bool enabled, const QString &location = QString());
    static bool iconDatabaseEnabled();

    static void setWebGraphic(WebGraphic type, const QPixmap &graphic);
    static QPixmap webGraphic(WebGraphic type);

private:
    friend class QWebPagePrivate;
    friend class QWebSettingsPrivate;

    Q_DISABLE_COPY(QWebSettings)

    QWebSettings();
    QWebSettings(WebCore::Settings *settings);
    ~QWebSettings();

    QWebSettingsPrivate *d;
};

#endif
