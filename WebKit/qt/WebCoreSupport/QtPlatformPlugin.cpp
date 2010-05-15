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

#include "config.h"
#include "QtPlatformPlugin.h"

#include "qwebkitplatformplugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>

namespace WebCore {

class SelectData : public QWebSelectData {
public:
    SelectData(QtAbstractWebPopup* data) : d(data) {}

    virtual ItemType itemType(int) const;
    virtual QString itemText(int index) const { return d->itemText(index); }
    virtual QString itemToolTip(int index) const { return d->itemToolTip(index); }
    virtual bool itemIsEnabled(int index) const { return d->itemIsEnabled(index); }
    virtual int itemCount() const { return d->itemCount(); }
    virtual bool itemIsSelected(int index) const { return d->itemIsSelected(index); }
    virtual bool multiple() const { return d->multiple(); }

private:
    QtAbstractWebPopup* d;
};

QWebSelectData::ItemType SelectData::itemType(int index) const
{
    switch (d->itemType(index)) {
    case QtAbstractWebPopup::Separator: return Separator;
    case QtAbstractWebPopup::Group: return Group;
    default: return Option;
    }
}

SelectInputMethodWrapper::SelectInputMethodWrapper(QWebSelectMethod* plugin)
    : m_plugin(plugin)
{
    m_plugin->setParent(this);
    connect(m_plugin, SIGNAL(didHide()), this, SLOT(didHide()));
    connect(m_plugin, SIGNAL(selectItem(int, bool, bool)), this, SLOT(selectItem(int, bool, bool)));
}

void SelectInputMethodWrapper::show()
{
    SelectData data(this);
    m_plugin->show(data);
}

void SelectInputMethodWrapper::hide()
{
    m_plugin->hide();
}

void SelectInputMethodWrapper::selectItem(int index, bool allowMultiplySelections, bool shift)
{
    QtAbstractWebPopup::selectItem(index, allowMultiplySelections, shift);
}

void SelectInputMethodWrapper::didHide()
{
    QtAbstractWebPopup::popupDidHide();
}

// QtPlatformPlugin

static QWebKitPlatformPlugin* getPluginObject()
{
    const QLatin1String suffix("/webkit/");
    const QStringList paths = QCoreApplication::libraryPaths();
    QObject* obj = 0;
    for (int i = 0; !obj && i < paths.count(); ++i) {
        const QDir dir(paths[i] + suffix);
        if (!dir.exists())
            continue;
        const QStringList files = dir.entryList(QDir::Files);
        for (int i = 0; i < files.count(); ++i) {
            QPluginLoader pluginLoader(dir.absoluteFilePath(files.at(i)));
            if (!pluginLoader.load())
                continue;
            obj = pluginLoader.instance();
            if (obj) {
                QWebKitPlatformPlugin* result = qobject_cast<QWebKitPlatformPlugin*>(obj);
                if (result)
                    return result;
                delete obj;
            }
            pluginLoader.unload();
        }
    }
    return 0;
}

QtPlatformPlugin::~QtPlatformPlugin()
{
    delete m_plugin;
}

QWebKitPlatformPlugin* QtPlatformPlugin::plugin()
{
    if (m_loaded)
        return m_plugin;

    m_loaded = true;
    m_plugin = getPluginObject();
    return m_plugin;
}

QtAbstractWebPopup* QtPlatformPlugin::createSelectInputMethod()
{
    QWebKitPlatformPlugin* p = plugin();
    if (!p)
        return 0;

    QWebSelectMethod* selector = p->createSelectInputMethod();
    if (!selector)
        return 0;

    return new SelectInputMethodWrapper(selector);
}

}
