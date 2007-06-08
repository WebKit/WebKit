/*
  Copyright (C) 2007 Trolltech ASA
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
  
  This class provides all functionality needed for loading images, style sheets and html
  pages from the web. It has a memory cache for these objects.
*/
#include "qwebobjectplugin_p.h"
#include <qwebobjectpluginconnector.h>
#include <qcoreapplication.h>
    
#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QWebFactoryLoader, loader,
                          (QWebObjectPluginFactoryInterface_iid, QCoreApplication::libraryPaths(), QLatin1String("/webplugins")))
#endif


QWebFactoryLoader::QWebFactoryLoader(const char *iid, const QStringList &paths, const QString &suffix, Qt::CaseSensitivity)
    : QFactoryLoader(iid, paths, suffix)
{
    QStringList plugins = keys();
    foreach(QString k, plugins) {
        QWebObjectPlugin *plugin = qobject_cast<QWebObjectPlugin *>(instance(k));
        if (!plugin)
            continue;
        QStringList extensions = plugin->extensionsForMimetype(k);
        foreach(QString ext, extensions) {
            m_extensions.append(ext);
            m_mimeTypesForExtension.append(k);
        }
    }
}

QWebFactoryLoader *QWebFactoryLoader::self()
{
    return loader();
}
    
QString QWebFactoryLoader::mimeTypeForExtension(const QString &extension)
{
    int idx = m_extensions.indexOf(extension);
    if (idx > 0)
        return m_mimeTypesForExtension.at(idx);
    return QString();
}

QObject *QWebFactoryLoader::create(QWebFrame *frame,
                                   const QUrl &url, 
                                   const QString &mimeType,
                                   const QStringList &argumentNames,
                                   const QStringList &argumentValues)
{
    QWebObjectPlugin *plugin = qobject_cast<QWebObjectPlugin *>(instance(mimeType));
    if (!plugin)
        return 0;
    QWebObjectPluginConnector *connector = new QWebObjectPluginConnector(frame);
    return plugin->create(connector, url, mimeType, argumentNames, argumentValues);
}



/*! \class QWebObjectPlugin

  This class is a plugin for the HTML object tag. It can be used to embed arbitrary content in a web page.
*/


QWebObjectPlugin::QWebObjectPlugin(QObject *parent)
    : QObject(parent)
{
}

QWebObjectPlugin::~QWebObjectPlugin()
{
}

/*!
  \fn QStringList QWebObjectPlugin::keys() const

  The keys are the mimetypes the plugin can handle
*/

/*!
  \fn QStringList QWebObjectPlugin::extensionsForMimetype() const

  Should return a list of extensions that are recognised to match the \a mimeType.
*/
QStringList QWebObjectPlugin::extensionsForMimetype(const QString &mimeType) const
{
    return QStringList();
}

/*!
  \fn QObject *QWebObjectPlugin::create(QWebObjectPluginConnector *connector, const QUrl &url, const QString &mimeType, const QStringList &argumentNames, const QStringList &argumentValues) const

  Creates a QObject with \a connector to handle \a mimeType. \a argumentNames and \a argumentValues are a set of key-value pairs passed directly
  from the &lt;param&gt; elements contained in the HTML object tag.
*/


