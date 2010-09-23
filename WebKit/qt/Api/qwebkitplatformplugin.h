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
#include <QUrl>

class QWebSelectData
{
public:
    virtual ~QWebSelectData() {}

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
    virtual ~QWebSelectMethod() {}

    virtual void show(const QWebSelectData&) = 0;
    virtual void hide() = 0;

Q_SIGNALS:
    void selectItem(int index, bool allowMultiplySelections, bool shift);
    void didHide();
};

class QWebNotificationData
{
public:
    virtual ~QWebNotificationData() {}

    virtual const QString title() const = 0;
    virtual const QString message() const = 0;
    virtual const QByteArray iconData() const = 0;
    virtual const QUrl openerPageUrl() const = 0;
};

class QWebNotificationPresenter : public QObject
{
    Q_OBJECT
public:
    QWebNotificationPresenter() {}
    virtual ~QWebNotificationPresenter() {}

    virtual void showNotification(const QWebNotificationData*) = 0;
    
Q_SIGNALS:
    void notificationClosed();
    void notificationClicked();
};

class QWebHapticFeedbackPlayer: public QObject
{
    Q_OBJECT
public:
    QWebHapticFeedbackPlayer() {}
    virtual ~QWebHapticFeedbackPlayer() {}

    enum HapticStrength {
        None, Weak, Medium, Strong
    };

    enum HapticEvent {
        Press, Release
    };

    virtual void playHapticFeedback(const HapticEvent, const QString& hapticType, const HapticStrength) = 0;
};

class QWebKitPlatformPlugin
{
public:
    virtual ~QWebKitPlatformPlugin() {}

    enum Extension {
        MultipleSelections,
        Notifications,
        Haptics
    };

    virtual bool supportsExtension(Extension extension) const = 0;
    virtual QObject* createExtension(Extension extension) const = 0;
};

Q_DECLARE_INTERFACE(QWebKitPlatformPlugin, "com.nokia.Qt.WebKit.PlatformPlugin/1.5");

#endif // QWEBKITPLATFORMPLUGIN_H
