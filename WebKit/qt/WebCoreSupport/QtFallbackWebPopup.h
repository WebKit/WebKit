/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
#ifndef QtFallbackWebPopup_h
#define QtFallbackWebPopup_h

#include "QtAbstractWebPopup.h"
#include <QComboBox>

#ifndef QT_NO_COMBOBOX

QT_BEGIN_NAMESPACE
class QGraphicsProxyWidget;
QT_END_NAMESPACE

namespace WebCore {

class QtFallbackWebPopupCombo;

class QtFallbackWebPopup : public QObject, public QtAbstractWebPopup {
    Q_OBJECT
public:
    QtFallbackWebPopup();
    ~QtFallbackWebPopup();

    virtual void show();
    virtual void hide();

    void destroyPopup();

private slots:
    void activeChanged(int);

private:
    friend class QtFallbackWebPopupCombo;
    bool m_popupVisible;
    QtFallbackWebPopupCombo* m_combo;

    void populate();
#if ENABLE(SYMBIAN_DIALOG_PROVIDERS)
    void showS60BrowserDialog();
#endif
};

class QtFallbackWebPopupCombo : public QComboBox {
public:
    QtFallbackWebPopupCombo(QtFallbackWebPopup& ownerPopup);
    virtual void showPopup();
    virtual void hidePopup();
    virtual bool eventFilter(QObject* watched, QEvent* event);

private:
    QtFallbackWebPopup& m_ownerPopup;
};

}

#endif // QT_NO_COMBOBOX

#endif // QtFallbackWebPopup_h
