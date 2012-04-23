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

#include "WKRetainPtr.h"
#include "WKStringQt.h"
#include "qwebpermissionrequest_p.h"
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

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

class BaseAuthenticationContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hostname READ hostname CONSTANT)
    Q_PROPERTY(QString prefilledUsername READ prefilledUsername CONSTANT)

public:
    BaseAuthenticationContextObject(const QString& hostname, const QString& prefilledUsername)
        : QObject()
        , m_hostname(hostname)
        , m_prefilledUsername(prefilledUsername)
    {
    }

    QString hostname() const { return m_hostname; }
    QString prefilledUsername() const { return m_prefilledUsername; }

public slots:
    void accept(const QString& username, const QString& password) { emit accepted(username, password); }
    void reject() { emit rejected(); }

signals:
    void accepted(const QString& username, const QString& password);
    void rejected();

private:
    QString m_hostname;
    QString m_prefilledUsername;
};

class HttpAuthenticationDialogContextObject : public BaseAuthenticationContextObject {
    Q_OBJECT
    Q_PROPERTY(QString realm READ realm CONSTANT)

public:
    HttpAuthenticationDialogContextObject(const QString& hostname, const QString& realm, const QString& prefilledUsername)
        : BaseAuthenticationContextObject(hostname, prefilledUsername)
        , m_realm(realm)
    {
    }

    QString realm() const { return m_realm; }

private:
    QString m_realm;
};

class ProxyAuthenticationDialogContextObject : public BaseAuthenticationContextObject {
    Q_OBJECT
    Q_PROPERTY(quint16 port READ port CONSTANT)

public:
    ProxyAuthenticationDialogContextObject(const QString& hostname, quint16 port, const QString& prefilledUsername)
        : BaseAuthenticationContextObject(hostname, prefilledUsername)
        , m_port(port)
    {
    }

    quint16 port() const { return m_port; }

private:
    quint16 m_port;
};

class CertificateVerificationDialogContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hostname READ hostname CONSTANT)

public:
    CertificateVerificationDialogContextObject(const QString& hostname)
        : QObject()
        , m_hostname(hostname)
    {
    }

    QString hostname() const { return m_hostname; }

public slots:
    void accept() { emit accepted(); }
    void reject() { emit rejected(); }

signals:
    void accepted();
    void rejected();

private:
    QString m_hostname;
};

class FilePickerContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList fileList READ fileList CONSTANT)
    Q_PROPERTY(bool allowMultipleFiles READ allowMultipleFiles CONSTANT)

public:
    FilePickerContextObject(const QStringList& selectedFiles, bool allowMultiple)
        : QObject()
        , m_allowMultiple(allowMultiple)
        , m_fileList(selectedFiles)
    {
    }

    QStringList fileList() const { return m_fileList; }
    bool allowMultipleFiles() const { return m_allowMultiple;}

public slots:
    void reject() { emit rejected();}
    void accept(const QVariant& path)
    {
        QStringList filesPath = path.toStringList();
        // For single file upload, send only the first element if there are more than one file paths
        if (!m_allowMultiple && filesPath.count() > 1)
            filesPath = QStringList(filesPath.at(0));
        emit fileSelected(filesPath);
    }

signals:
    void rejected();
    void fileSelected(const QStringList&);

private:
    bool m_allowMultiple;
    QStringList m_fileList;
};

class DatabaseQuotaDialogContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString databaseName READ databaseName CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(quint64 currentQuota READ currentQuota CONSTANT)
    Q_PROPERTY(quint64 currentOriginUsage READ currentOriginUsage CONSTANT)
    Q_PROPERTY(quint64 currentDatabaseUsage READ currentDatabaseUsage CONSTANT)
    Q_PROPERTY(quint64 expectedUsage READ expectedUsage CONSTANT)
    Q_PROPERTY(QtWebSecurityOrigin* origin READ securityOrigin CONSTANT)

public:
    DatabaseQuotaDialogContextObject(const QString& databaseName, const QString& displayName, WKSecurityOriginRef securityOrigin, quint64 currentQuota, quint64 currentOriginUsage, quint64 currentDatabaseUsage, quint64 expectedUsage)
        : QObject()
        , m_databaseName(databaseName)
        , m_displayName(displayName)
        , m_currentQuota(currentQuota)
        , m_currentOriginUsage(currentOriginUsage)
        , m_currentDatabaseUsage(currentDatabaseUsage)
        , m_expectedUsage(expectedUsage)
    {
        WKRetainPtr<WKStringRef> scheme = adoptWK(WKSecurityOriginCopyProtocol(securityOrigin));
        WKRetainPtr<WKStringRef> host = adoptWK(WKSecurityOriginCopyHost(securityOrigin));

        m_securityOrigin.setScheme(WKStringCopyQString(scheme.get()));
        m_securityOrigin.setHost(WKStringCopyQString(host.get()));
        m_securityOrigin.setPort(static_cast<int>(WKSecurityOriginGetPort(securityOrigin)));
    }

    QString databaseName() const { return m_databaseName; }
    QString displayName() const { return m_displayName; }
    quint64 currentQuota() const { return m_currentQuota; }
    quint64 currentOriginUsage() const { return m_currentOriginUsage; }
    quint64 currentDatabaseUsage() const { return m_currentDatabaseUsage; }
    quint64 expectedUsage() const { return m_expectedUsage; }
    QtWebSecurityOrigin* securityOrigin() { return &m_securityOrigin; }

public slots:
    void accept(quint64 size) { emit accepted(size); }
    void reject() { emit rejected(); }

signals:
    void accepted(quint64 size);
    void rejected();

private:
    QString m_databaseName;
    QString m_displayName;
    quint64 m_currentQuota;
    quint64 m_currentOriginUsage;
    quint64 m_currentDatabaseUsage;
    quint64 m_expectedUsage;
    QtWebSecurityOrigin m_securityOrigin;
};

bool QtDialogRunner::initForAlert(QQmlComponent* component, QQuickItem* dialogParent, const QString& message)
{
    DialogContextObject* contextObject = new DialogContextObject(message);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(dismissed()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForConfirm(QQmlComponent* component, QQuickItem* dialogParent, const QString& message)
{
    DialogContextObject* contextObject = new DialogContextObject(message);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted()), SLOT(onAccepted()));
    connect(contextObject, SIGNAL(accepted()), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForPrompt(QQmlComponent* component, QQuickItem* dialogParent, const QString& message, const QString& defaultValue)
{
    DialogContextObject* contextObject = new DialogContextObject(message, defaultValue);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(QString)), SLOT(onAccepted(QString)));
    connect(contextObject, SIGNAL(accepted(QString)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));
    return true;
}

bool QtDialogRunner::initForAuthentication(QQmlComponent* component, QQuickItem* dialogParent, const QString& hostname, const QString& realm, const QString& prefilledUsername)
{
    HttpAuthenticationDialogContextObject* contextObject = new HttpAuthenticationDialogContextObject(hostname, realm, prefilledUsername);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(onAuthenticationAccepted(QString, QString)));
    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::initForProxyAuthentication(QQmlComponent* component, QQuickItem* dialogParent, const QString& hostname, uint16_t port, const QString& prefilledUsername)
{
    ProxyAuthenticationDialogContextObject* contextObject = new ProxyAuthenticationDialogContextObject(hostname, port, prefilledUsername);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(onAuthenticationAccepted(QString, QString)));
    connect(contextObject, SIGNAL(accepted(QString, QString)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::initForCertificateVerification(QQmlComponent* component, QQuickItem* dialogParent, const QString& hostname)
{
    CertificateVerificationDialogContextObject* contextObject = new CertificateVerificationDialogContextObject(hostname);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted()), SLOT(onAccepted()));
    connect(contextObject, SIGNAL(accepted()), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::initForFilePicker(QQmlComponent* component, QQuickItem* dialogParent, const QStringList& selectedFiles, bool allowMultiple)
{
    FilePickerContextObject* contextObject = new FilePickerContextObject(selectedFiles, allowMultiple);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(fileSelected(QStringList)), SLOT(onFileSelected(QStringList)));
    connect(contextObject, SIGNAL(fileSelected(QStringList)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::initForDatabaseQuotaDialog(QQmlComponent* component, QQuickItem* dialogParent, const QString& databaseName, const QString& displayName, WKSecurityOriginRef securityOrigin, quint64 currentQuota, quint64 currentOriginUsage, quint64 currentDatabaseUsage, quint64 expectedUsage)
{
    DatabaseQuotaDialogContextObject* contextObject = new DatabaseQuotaDialogContextObject(databaseName, displayName, securityOrigin, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage);
    if (!createDialog(component, dialogParent, contextObject))
        return false;

    connect(contextObject, SIGNAL(accepted(quint64)), SLOT(onDatabaseQuotaAccepted(quint64)));
    connect(contextObject, SIGNAL(accepted(quint64)), SLOT(quit()));
    connect(contextObject, SIGNAL(rejected()), SLOT(quit()));

    return true;
}

bool QtDialogRunner::createDialog(QQmlComponent* component, QQuickItem* dialogParent, QObject* contextObject)
{
    QQmlContext* baseContext = component->creationContext();
    if (!baseContext)
        baseContext = QQmlEngine::contextForObject(dialogParent);
    m_dialogContext = adoptPtr(new QQmlContext(baseContext));

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

} // namespace WebKit

#include "QtDialogRunner.moc"
#include "moc_QtDialogRunner.cpp"

