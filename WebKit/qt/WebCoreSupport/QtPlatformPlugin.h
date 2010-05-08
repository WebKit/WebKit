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

#ifndef QtPlatformPlugin_h
#define QtPlatformPlugin_h

#include "QtAbstractWebPopup.h"
#include <QObject>

class QWebSelectMethod;
class QWebKitPlatformPlugin;

namespace WebCore {

class SelectInputMethodWrapper : public QObject, public QtAbstractWebPopup {
    Q_OBJECT
public:
    SelectInputMethodWrapper(QWebSelectMethod* plugin);

    virtual void show();
    virtual void hide();

private Q_SLOTS:
    void selectItem(int index, bool allowMultiplySelections, bool shift);
    void didHide();

private:
    QWebSelectMethod* m_plugin;
};

class QtPlatformPlugin {
public:
    QtPlatformPlugin() : m_loaded(false), m_plugin(0) {}
    ~QtPlatformPlugin();

    QtAbstractWebPopup* createSelectInputMethod();

private:
    bool m_loaded;
    QWebKitPlatformPlugin* m_plugin;

    QWebKitPlatformPlugin* plugin();
};

}

#endif // QtPlatformPlugin_h
