/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef QWEBKITPLATFORMPLUGIN_H
#define QWEBKITPLATFORMPLUGIN_H

/*
 *  Warning: The contents of this file is not  part of the public QtWebKit API
 *  and may be changed from version to version or even be completely removed.
*/

#include <QObject>

class QWebSelectData
{
public:
    inline ~QWebSelectData() {}

    enum ItemType { Option, Group, Separator };

    virtual ItemType itemType(int) const = 0;
    virtual QString itemText(int index) const = 0;
    virtual QString itemToolTip(int index) const = 0;
    virtual bool itemIsEnabled(int index) const = 0;
    virtual bool itemIsSelected(int index) const = 0;
    virtual int itemCount() const = 0;
    virtual bool multiple() const = 0;
};

class QWebSelectMethod : public QObject
{
    Q_OBJECT
public:
    inline ~QWebSelectMethod() {}

    virtual void show(const QWebSelectData&) = 0;
    virtual void hide() = 0;

Q_SIGNALS:
    void selectItem(int index, bool allowMultiplySelections, bool shift);
    void didHide();
};

class QWebKitPlatformPlugin
{
public:
    inline ~QWebKitPlatformPlugin() {}

    enum Extension {
        MultipleSelections
    };

    virtual QWebSelectMethod* createSelectInputMethod() const = 0;
    virtual bool supportsExtension(Extension extension) const = 0;

};

Q_DECLARE_INTERFACE(QWebKitPlatformPlugin, "com.nokia.Qt.WebKit.PlatformPlugin/1.0");

#endif // QWEBKITPLATFORMPLUGIN_H
