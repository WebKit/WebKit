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

#include "qwebinspector.h"
#include "qwebinspector_p.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebview.h"

#include <QtCore/QSettings>
#include <QtCore/QCoreApplication>

#include "InspectorController.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformString.h"

namespace WebCore {

static const QLatin1String settingStoragePrefix("Qt/QtWebKit/QWebInspector/");
static const QLatin1String settingStorageTypeSuffix(".type");

static InspectorController::Setting variantToSetting(const QVariant& qvariant);
static QVariant settingToVariant(const InspectorController::Setting& icSetting);

class InspectorClientWebPage : public QWebPage {
    Q_OBJECT
    friend class InspectorClientQt;
public:
    InspectorClientWebPage(QObject* parent = 0)
    : QWebPage(parent)
    {
        settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, false);
    }

    QWebPage* createWindow(QWebPage::WebWindowType)
    {
        QWebView* view = new QWebView;
        QWebPage* page = new QWebPage;
        view->setPage(page);
        view->setAttribute(Qt::WA_DeleteOnClose);
        return page;
    }
};

InspectorClientQt::InspectorClientQt(QWebPage* page)
    : m_inspectedWebPage(page)
{}

void InspectorClientQt::inspectorDestroyed()
{
    delete this;
}

Page* InspectorClientQt::createPage()
{
    if (m_inspectorView)
        return m_inspectorView->page()->d->page;

    QWebView* inspectorView = new QWebView;
    InspectorClientWebPage* inspectorPage = new InspectorClientWebPage(inspectorView);
    inspectorView->setPage(inspectorPage);
    m_inspectorView.set(inspectorView);

    inspectorPage->mainFrame()->load(QString::fromLatin1("qrc:/webkit/inspector/inspector.html"));
    m_inspectedWebPage->d->inspectorFrontend = inspectorView;
    m_inspectedWebPage->d->getOrCreateInspector()->d->setFrontend(inspectorView);

    return m_inspectorView->page()->d->page;
}

String InspectorClientQt::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorClientQt::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorClientQt::showWindow()
{
    updateWindowTitle();

    m_inspectedWebPage->d->inspectorController()->setWindowVisible(true, true);
    // We don't allow the inspector to ask for widget visibility itself because showWindow is
    // not always called when we want.
    // Inspecting an element or calling QWebInspector::show() should already have made the
    // widget visible.
}

void InspectorClientQt::closeWindow()
{
    if (m_inspectedWebPage->d->inspector)
        m_inspectedWebPage->d->inspector->close();
}

void InspectorClientQt::attachWindow()
{
    notImplemented();
}

void InspectorClientQt::detachWindow()
{
    notImplemented();
}

void InspectorClientQt::setAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void InspectorClientQt::highlight(Node*)
{
    notImplemented();
}

void InspectorClientQt::hideHighlight()
{
    notImplemented();
}

void InspectorClientQt::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void InspectorClientQt::inspectorWindowObjectCleared()
{
    notImplemented();
}

void InspectorClientQt::updateWindowTitle()
{
    if (m_inspectedWebPage->d->inspector) {
        QString caption = QCoreApplication::translate("QWebPage", "Web Inspector - %2").arg(m_inspectedURL);
        m_inspectedWebPage->d->inspector->setWindowTitle(caption);
        emit m_inspectedWebPage->d->inspector->windowTitleChanged(caption);
    }
}

void InspectorClientQt::populateSetting(const String& key, InspectorController::Setting& setting)
{
    QSettings qsettings;
    if (qsettings.status() == QSettings::AccessError) {
        // QCoreApplication::setOrganizationName and QCoreApplication::setApplicationName haven't been called
        qWarning("QWebInspector: QSettings couldn't read configuration setting [%s].",
                 qPrintable(static_cast<QString>(key)));
        return;
    }

    QString settingKey(settingStoragePrefix + key);
    QString storedValueType = qsettings.value(settingKey + settingStorageTypeSuffix).toString();
    QVariant storedValue = qsettings.value(settingKey);
    storedValue.convert(QVariant::nameToType(storedValueType.toAscii().data()));
    setting = variantToSetting(storedValue);
}

void InspectorClientQt::storeSetting(const String& key, const InspectorController::Setting& setting)
{
    QSettings qsettings;
    if (qsettings.status() == QSettings::AccessError) {
        qWarning("QWebInspector: QSettings couldn't persist configuration setting [%s].",
                 qPrintable(static_cast<QString>(key)));
        return;
    }

    QVariant valueToStore = settingToVariant(setting);
    QString settingKey(settingStoragePrefix + key);
    qsettings.setValue(settingKey, valueToStore);
    qsettings.setValue(settingKey + settingStorageTypeSuffix, QVariant::typeToName(valueToStore.type()));
}

void InspectorClientQt::removeSetting(const String&)
{
    notImplemented();
}

static InspectorController::Setting variantToSetting(const QVariant& qvariant)
{
    InspectorController::Setting retVal;

    switch (qvariant.type()) {
    case QVariant::Bool:
        retVal.set(qvariant.toBool());
        break;
    case QVariant::Double:
        retVal.set(qvariant.toDouble());
        break;
    case QVariant::Int:
        retVal.set((long)qvariant.toInt());
        break;
    case QVariant::String:
        retVal.set(qvariant.toString());
        break;
    case QVariant::StringList: {
        QStringList qsList = qvariant.toStringList();
        int listCount = qsList.count();
        Vector<String> vector(listCount);
        for (int i = 0; i < listCount; ++i)
            vector[i] = qsList[i];
        retVal.set(vector);
        break;
    }
    }

    return retVal;
}

static QVariant settingToVariant(const InspectorController::Setting& icSetting)
{
    QVariant retVal;

    switch (icSetting.type()) {
    case InspectorController::Setting::StringType:
        retVal.setValue(static_cast<QString>(icSetting.string()));
        break;
    case InspectorController::Setting::StringVectorType: {
        const Vector<String>& vector = icSetting.stringVector();
        Vector<String>::const_iterator iter;
        QStringList qsList;
        for (iter = vector.begin(); iter != vector.end(); ++iter)
            qsList << *iter;
        retVal.setValue(qsList);
        break;
    }
    case InspectorController::Setting::DoubleType:
        retVal.setValue(icSetting.doubleValue());
        break;
    case InspectorController::Setting::IntegerType:
        retVal.setValue((int)icSetting.integerValue());
        break;
    case InspectorController::Setting::BooleanType:
        retVal.setValue(icSetting.booleanValue());
        break;
    }

    return retVal;
}

}

#include "InspectorClientQt.moc"
