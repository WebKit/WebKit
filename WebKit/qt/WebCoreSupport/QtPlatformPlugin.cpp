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

bool QtPlatformPlugin::load(const QString& file)
{
    m_loader.setFileName(file);
    if (!m_loader.load())
        return false;

    QObject* obj = m_loader.instance();
    if (obj) {
        m_plugin = qobject_cast<QWebKitPlatformPlugin*>(obj);
        if (m_plugin)
            return true;
    }

    m_loader.unload();
    return false;
}

bool QtPlatformPlugin::load()
{
    const QLatin1String suffix("/webkit/");
    const QStringList paths = QCoreApplication::libraryPaths();

    for (int i = 0; i < paths.count(); ++i) {
        const QDir dir(paths[i] + suffix);
        if (!dir.exists())
            continue;
        const QStringList files = dir.entryList(QDir::Files);
        for (int j = 0; j < files.count(); ++j) {
            if (load(dir.absoluteFilePath(files.at(j))))
                return true;
        }
    }
    return false;
}

QtPlatformPlugin::~QtPlatformPlugin()
{
    m_loader.unload();
}

QWebKitPlatformPlugin* QtPlatformPlugin::plugin()
{
    if (m_loaded)
        return m_plugin;
    m_loaded = true;

    // Plugin path is stored in a static variable to avoid searching for the plugin
    // more then once.
    static QString pluginPath;

    if (pluginPath.isNull()) {
        if (load())
            pluginPath = m_loader.fileName();
    } else
        load(pluginPath);

    return m_plugin;
}

QWebSelectMethod* QtPlatformPlugin::createSelectInputMethod()
{
    QWebKitPlatformPlugin* p = plugin();
    return p ? qobject_cast<QWebSelectMethod*>(p->createExtension(QWebKitPlatformPlugin::MultipleSelections)) : 0;
}


QWebNotificationPresenter* QtPlatformPlugin::createNotificationPresenter()
{
    QWebKitPlatformPlugin* p = plugin();
    return p ? qobject_cast<QWebNotificationPresenter*>(p->createExtension(QWebKitPlatformPlugin::Notifications)) : 0;
}

}
