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

#include "config.h"
#include "QtDialogRunner.h"

#include <QtDeclarative/QDeclarativeComponent>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtQuick/QQuickItem>
#include <wtf/PassOwnPtr.h>

QtDialogRunner::QtDialogRunner()
    : QEventLoop()
    , m_wasAccepted(false)
{
}

QtDialogRunner::~QtDialogRunner()
{
}

class DialogContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString message READ message CONSTANT)
    Q_PROPERTY(QString defaultValue READ defaultValue CONSTANT)

public:
    DialogContextObject(const QString& message, const QString& defaultValue = QString())
        : QObject()
        , m_message(message)
        , m_defaultValue(defaultValue)
    {
    }
    QString message() const { return m_message; }
    QString defaultValue() const { return m_defaultValue; }

public slots:
    void dismiss() { emit dismissed(); }
    void accept() { emit accepted(); }
    void accept(const QString& result) { emit accepted(result); }
    void reject() { emit rejected(); }

signals:
    void dismissed();
    void accepted();
    void accepted(const QString& result);
    void rejected();

private:
    QString m_message;
    QString m_defaultValue;
};

class AuthenticationDialogContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hostname READ hostname CONSTANT)
    Q_PROPERTY(QString realm READ realm CONSTANT)
    Q_PROPERTY(QString prefilledUsername READ prefilledUsername CONSTANT)

public:
    AuthenticationDialogContextObject(const QString& hostname, const QString& realm, const QString& prefilledUsername)
        : QObject()
        , m_hostname(hostname)
        , m_realm(realm)
        , m_prefilledUsername(prefilledUsername)
    {
    }

    QString hostname() const { return m_hostname; }
    QString realm() const { return m_realm; }
    QString prefilledUsername() const { return m_prefilledUsername; }

public slots:
    void accept(const QString& username, const QString& password) { emit accepted(username, password); }
    void reject() { emit rejected(); }

signals:
    void accepted(const QString& username, const QString& password);
    void rejected();

private:
    QString m_hostname;
    QString m_realm;
    QString m_prefilledUsername;
};

bool QtDialogRunner::initForAlert(QDeclarativeComponent* component, QQuickItem* dialogParent, const QString& message)
{
    DialogContextObject* contextObject = new DialogContextObject(message);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(dismissed()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForConfirm(QDeclarativeComponent* component, QQuickItem* dialogParent, const QString& message)
{
    DialogContextObject* contextObject = new DialogContextObject(message);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted()), SLOT(onAccepted()));
    connect(contextObject, SIGNAL(accepted()), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForPrompt(QDeclarativeComponent* component, QQuickItem* dialogParent, const QString& message, const QString& defaultValue)
{
    DialogContextObject* contextObject = new DialogContextObject(message, defaultValue);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(QString)), SLOT(onAccepted(QString)));
    connect(contextObject, SIGNAL(accepted(QString)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForAuthentication(QDeclarativeComponent* component, QQuickItem* dialogParent, const QString& hostname, const QString& realm, const QString& prefilledUsername)
{
    AuthenticationDialogContextObject* contextObject = new AuthenticationDialogContextObject(hostname, realm, prefilledUsername);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(onAuthenticationAccepted(QString, QString)));
    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::createDialog(QDeclarativeComponent* component, QQuickItem* dialogParent, QObject* contextObject)
{
    QDeclarativeContext* baseContext = component->creationContext();
    if (!baseContext)
        baseContext = QDeclarativeEngine::contextForObject(dialogParent);
    m_dialogContext = adoptPtr(new QDeclarativeContext(baseContext));

    // This makes both "message" and "model.message" work for the dialog, just like QtQuick's ListView delegates.
    contextObject->setParent(m_dialogContext.get());
    m_dialogContext->setContextProperty(QLatin1String("model"), contextObject);
    m_dialogContext->setContextObject(contextObject);

    QObject* object = component->create(m_dialogContext.get());
    if (!object) {
        m_dialogContext.clear();
        return false;
    }

    m_dialog = adoptPtr(qobject_cast<QQuickItem*>(object));
    if (!m_dialog) {
        m_dialogContext.clear();
        m_dialog.clear();
        return false;
    }

    m_dialog->setParentItem(dialogParent);
    return true;
}

#include "QtDialogRunner.moc"
#include "moc_QtDialogRunner.cpp"
