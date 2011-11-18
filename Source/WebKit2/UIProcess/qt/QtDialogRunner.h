/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef QtDialogRunner_h
#define QtDialogRunner_h

#include <QtCore/QEventLoop>
#include <wtf/OwnPtr.h>

class QDeclarativeComponent;
class QDeclarativeContext;
class QQuickItem;

class QtDialogRunner : public QEventLoop {
    Q_OBJECT

public:
    QtDialogRunner();
    virtual ~QtDialogRunner();

    bool initForAlert(QDeclarativeComponent*, QQuickItem* dialogParent, const QString& message);
    bool initForConfirm(QDeclarativeComponent*, QQuickItem* dialogParent, const QString& message);
    bool initForPrompt(QDeclarativeComponent*, QQuickItem* dialogParent, const QString& message, const QString& defaultValue);

    bool wasAccepted() const { return m_wasAccepted; }
    QString result() const { return m_result; }

public slots:
    void onAccepted(const QString& result = QString())
    {
        m_wasAccepted = true;
        m_result = result;
    }

private:
    bool createDialog(QDeclarativeComponent*, QQuickItem* dialogParent, QObject* contextObject);

    OwnPtr<QDeclarativeContext> m_dialogContext;
    OwnPtr<QQuickItem> m_dialog;
    QString m_result;
    bool m_wasAccepted;
};

#endif // QtDialogRunner_h
