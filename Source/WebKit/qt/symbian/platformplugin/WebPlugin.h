/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 
#ifndef WebPlugin_h
#define WebPlugin_h

#include "qwebkitplatformplugin.h"
#include <QDialog>

class QListWidgetItem;
class QListWidget;
class QScrollArea;

class PopupScrollingHandler;

class Popup : public QDialog {
    Q_OBJECT
public:
    Popup(const QWebSelectData&);
    void updateSelectionsBeforeDialogClosing();
    QList<int> preSelectedIndices() const { return m_preSelectedIndices; }
    QListWidget* listWidget() { return m_list; }

signals:
    void itemClicked(int idx);

protected slots:
    void onItemSelected(QListWidgetItem*);
    void updateAndClose();

protected:
    void populateList();
    void resizeEvent(QResizeEvent*);
    const QWebSelectData& m_data;
    QListWidget* m_list;
    QList<int> m_preSelectedIndices;
    PopupScrollingHandler* m_scrollingHandler;
};


class SingleSelectionPopup : public Popup {
    Q_OBJECT
public:
    SingleSelectionPopup(const QWebSelectData&);
};


class MultipleSelectionPopup : public Popup {
    Q_OBJECT
public:
    MultipleSelectionPopup(const QWebSelectData&);
};


class WebPopup : public QWebSelectMethod {
    Q_OBJECT
public:
    WebPopup();
    ~WebPopup();

    virtual void show(const QWebSelectData&);
    virtual void hide();

private slots:
    void popupClosed();
    void itemClicked(int idx);

private:
    Popup* m_popup;

    Popup* createPopup(const QWebSelectData&);
    Popup* createSingleSelectionPopup(const QWebSelectData&);
    Popup* createMultipleSelectionPopup(const QWebSelectData&);
};

class WebNotificationPresenter : public QWebNotificationPresenter {
    Q_OBJECT
public:
    WebNotificationPresenter() { }
    ~WebNotificationPresenter() { }
    void showNotification(const QWebNotificationData*);
};

class WebPlugin : public QObject, public QWebKitPlatformPlugin {
    Q_OBJECT
    Q_INTERFACES(QWebKitPlatformPlugin)
public:
    virtual bool supportsExtension(Extension) const;
    virtual QObject* createExtension(Extension) const;
};

#endif // WebPlugin_h
