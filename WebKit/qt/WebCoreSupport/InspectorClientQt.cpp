/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientQt.h"

#include "Frame.h"
#include "InspectorController.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "qwebinspector.h"
#include "qwebinspector_p.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebview.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QVariant>

namespace WebCore {

static const QLatin1String settingStoragePrefix("Qt/QtWebKit/QWebInspector/");
static const QLatin1String settingStorageTypeSuffix(".type");

static String variantToSetting(const QVariant& qvariant);
static QVariant settingToVariant(const String& value);

class InspectorClientWebPage : public QWebPage {
    Q_OBJECT
    friend class InspectorClientQt;
public:
    InspectorClientWebPage(QObject* parent = 0)
        : QWebPage(parent)
    {
        connect(mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), SLOT(javaScriptWindowObjectCleared()));
    }

    QWebPage* createWindow(QWebPage::WebWindowType)
    {
        QWebView* view = new QWebView;
        QWebPage* page = new QWebPage;
        view->setPage(page);
        view->setAttribute(Qt::WA_DeleteOnClose);
        return page;
    }

public slots:
    void javaScriptWindowObjectCleared() 
    {
#ifndef QT_NO_PROPERTIES
        QVariant inspectorJavaScriptWindowObjects = property("_q_inspectorJavaScriptWindowObjects");
        if (!inspectorJavaScriptWindowObjects.isValid())
            return;
        QMap<QString, QVariant> javaScriptNameObjectMap = inspectorJavaScriptWindowObjects.toMap();
        QWebFrame* frame = mainFrame();
        QMap<QString, QVariant>::const_iterator it = javaScriptNameObjectMap.constBegin();
        for ( ; it != javaScriptNameObjectMap.constEnd(); ++it) {
            QString name = it.key();
            QVariant value = it.value();
            QObject* obj = value.value<QObject*>();
            frame->addToJavaScriptWindowObject(name, obj);
        }
#endif
    }
};

InspectorClientQt::InspectorClientQt(QWebPage* page)
    : m_inspectedWebPage(page)
    , m_frontendWebPage(0)
    , m_frontendClient(0)
{}

void InspectorClientQt::inspectorDestroyed()
{
    if (m_frontendClient)
        m_frontendClient->inspectorClientDestroyed();
    delete this;
}

    
void InspectorClientQt::openInspectorFrontend(WebCore::InspectorController*)
{
    QWebView* inspectorView = new QWebView;
    InspectorClientWebPage* inspectorPage = new InspectorClientWebPage(inspectorView);
    inspectorView->setPage(inspectorPage);

    QWebInspector* inspector = m_inspectedWebPage->d->getOrCreateInspector();
    // This is a known hook that allows changing the default URL for the
    // Web inspector. This is used for SDK purposes. Please keep this hook
    // around and don't remove it.
    // https://bugs.webkit.org/show_bug.cgi?id=35340
    QUrl inspectorUrl;
#ifndef QT_NO_PROPERTIES
    inspectorUrl = inspector->property("_q_inspectorUrl").toUrl();
#endif
    if (!inspectorUrl.isValid())
        inspectorUrl = QUrl("qrc:/webkit/inspector/inspector.html");

#ifndef QT_NO_PROPERTIES
    QVariant inspectorJavaScriptWindowObjects = inspector->property("_q_inspectorJavaScriptWindowObjects");
    if (inspectorJavaScriptWindowObjects.isValid())
        inspectorPage->setProperty("_q_inspectorJavaScriptWindowObjects", inspectorJavaScriptWindowObjects);
#endif
    inspectorView->page()->mainFrame()->load(inspectorUrl);
    m_inspectedWebPage->d->inspectorFrontend = inspectorView;
    inspector->d->setFrontend(inspectorView);

    m_frontendClient = new InspectorFrontendClientQt(m_inspectedWebPage, inspectorView, this);
    inspectorView->page()->d->page->inspectorController()->setInspectorFrontendClient(m_frontendClient);
    m_frontendWebPage = inspectorPage;
}

void InspectorClientQt::releaseFrontendPage()
{
    m_frontendWebPage = 0;
    m_frontendClient = 0;
}

void InspectorClientQt::highlight(Node*)
{
    notImplemented();
}

void InspectorClientQt::hideHighlight()
{
    notImplemented();
}

void InspectorClientQt::populateSetting(const String& key, String* setting)
{
#ifdef QT_NO_SETTINGS
    Q_UNUSED(key)
    Q_UNUSED(setting)
    qWarning("QWebInspector: QSettings is not supported by Qt.");
#else
    QSettings qsettings;
    if (qsettings.status() == QSettings::AccessError) {
        // QCoreApplication::setOrganizationName and QCoreApplication::setApplicationName haven't been called
        qWarning("QWebInspector: QSettings couldn't read configuration setting [%s].",
                 qPrintable(static_cast<QString>(key)));
        return;
    }

    QString settingKey(settingStoragePrefix + QString(key));
    QString storedValueType = qsettings.value(settingKey + settingStorageTypeSuffix).toString();
    QVariant storedValue = qsettings.value(settingKey);
    storedValue.convert(QVariant::nameToType(storedValueType.toAscii().data()));
    *setting = variantToSetting(storedValue);
#endif // QT_NO_SETTINGS
}

void InspectorClientQt::storeSetting(const String& key, const String& setting)
{
#ifdef QT_NO_SETTINGS
    Q_UNUSED(key)
    Q_UNUSED(setting)
    qWarning("QWebInspector: QSettings is not supported by Qt.");
#else
    QSettings qsettings;
    if (qsettings.status() == QSettings::AccessError) {
        qWarning("QWebInspector: QSettings couldn't persist configuration setting [%s].",
                 qPrintable(static_cast<QString>(key)));
        return;
    }

    QVariant valueToStore = settingToVariant(setting);
    QString settingKey(settingStoragePrefix + QString(key));
    qsettings.setValue(settingKey, valueToStore);
    qsettings.setValue(settingKey + settingStorageTypeSuffix, QVariant::typeToName(valueToStore.type()));
#endif // QT_NO_SETTINGS
}

bool InspectorClientQt::sendMessageToFrontend(const String& message)
{
    if (!m_frontendWebPage)
        return false;

    Page* frontendPage = QWebPagePrivate::core(m_frontendWebPage);
    if (!frontendPage)
        return false;

    Frame* frame = frontendPage->mainFrame();
    if (!frame)
        return false;

    ScriptController* scriptController = frame->script();
    if (!scriptController)
        return false;

    String dispatchToFrontend("WebInspector.dispatchMessageFromBackend(");
    dispatchToFrontend += message;
    dispatchToFrontend += ");";
    scriptController->executeScript(dispatchToFrontend);
    return true;
}

static String variantToSetting(const QVariant& qvariant)
{
    String retVal;

    switch (qvariant.type()) {
    case QVariant::Bool:
        retVal = qvariant.toBool() ? "true" : "false";
    case QVariant::String:
        retVal = qvariant.toString();
    default:
        break;
    }

    return retVal;
}

static QVariant settingToVariant(const String& setting)
{
    QVariant retVal;
    retVal.setValue(static_cast<QString>(setting));
    return retVal;
}

InspectorFrontendClientQt::InspectorFrontendClientQt(QWebPage* inspectedWebPage, PassOwnPtr<QWebView> inspectorView, InspectorClientQt* inspectorClient)
    : InspectorFrontendClientLocal(inspectedWebPage->d->page->inspectorController(), inspectorView->page()->d->page) 
    , m_inspectedWebPage(inspectedWebPage)
    , m_inspectorView(inspectorView)
    , m_destroyingInspectorView(false)
    , m_inspectorClient(inspectorClient)
{
}

InspectorFrontendClientQt::~InspectorFrontendClientQt()
{
    ASSERT(m_destroyingInspectorView);
    if (m_inspectorClient)
        m_inspectorClient->releaseFrontendPage();
}

void InspectorFrontendClientQt::frontendLoaded()
{
    InspectorFrontendClientLocal::frontendLoaded();
    setAttachedWindow(true);
}

String InspectorFrontendClientQt::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorFrontendClientQt::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorFrontendClientQt::bringToFront()
{
    updateWindowTitle();
}

void InspectorFrontendClientQt::closeWindow()
{
    destroyInspectorView(true);
}

void InspectorFrontendClientQt::disconnectFromBackend()
{
    destroyInspectorView(false);
}

void InspectorFrontendClientQt::attachWindow()
{
    notImplemented();
}

void InspectorFrontendClientQt::detachWindow()
{
    notImplemented();
}

void InspectorFrontendClientQt::setAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void InspectorFrontendClientQt::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void InspectorFrontendClientQt::updateWindowTitle()
{
    if (m_inspectedWebPage->d->inspector) {
        QString caption = QCoreApplication::translate("QWebPage", "Web Inspector - %2").arg(m_inspectedURL);
        m_inspectedWebPage->d->inspector->setWindowTitle(caption);
    }
}

void InspectorFrontendClientQt::destroyInspectorView(bool notifyInspectorController)
{
    if (m_destroyingInspectorView)
        return;
    m_destroyingInspectorView = true;

    // Inspected page may have already been destroyed.
    if (m_inspectedWebPage) {
        // Clear reference from QWebInspector to the frontend view.
        m_inspectedWebPage->d->getOrCreateInspector()->d->setFrontend(0);
    }

#if ENABLE(INSPECTOR)
    if (notifyInspectorController)
        m_inspectedWebPage->d->inspectorController()->disconnectFrontend();
#endif
    if (m_inspectorClient)
        m_inspectorClient->releaseFrontendPage();

    // Clear pointer before deleting WebView to avoid recursive calls to its destructor.
    OwnPtr<QWebView> inspectorView = m_inspectorView.release();
}

void InspectorFrontendClientQt::inspectorClientDestroyed()
{
    m_inspectorClient = 0;
    m_inspectedWebPage = 0;
}

}

#include "InspectorClientQt.moc"
