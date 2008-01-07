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
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/
#ifndef QWEBOBJECTPLUGIN_H
#define QWEBOBJECTPLUGIN_H

#include "qwebkitglobal.h"
#include <QtCore/qplugin.h>
#include <QtCore/qfactoryinterface.h>

#if QT_VERSION < 0x040400

class QWebObjectPluginConnector;
class QUrl;

struct QWEBKIT_EXPORT QWebObjectPluginFactoryInterface : public QFactoryInterface
{
    virtual QObject *create(QWebObjectPluginConnector *connector,
                            const QUrl &url,
                            const QString &mimeType,
                            const QStringList &argumentNames,
                            const QStringList &argumentValues) const = 0;
};

#define QWebObjectPluginFactoryInterface_iid "com.trolltech.Qt.QWebObjectPluginFactoryInterface"
Q_DECLARE_INTERFACE(QWebObjectPluginFactoryInterface, QWebObjectPluginFactoryInterface_iid)

class QWEBKIT_EXPORT QWebObjectPlugin : public QObject, public QWebObjectPluginFactoryInterface
{
    Q_OBJECT
    Q_INTERFACES(QWebObjectPluginFactoryInterface:QFactoryInterface)
public:
    explicit QWebObjectPlugin(QObject *parent = 0);
    virtual ~QWebObjectPlugin();

    virtual QStringList keys() const = 0;

    virtual QString descriptionForKey(const QString &key) const;
    virtual QStringList mimetypesForKey(const QString &key) const;
    virtual QStringList extensionsForMimetype(const QString &mimeType) const;
    virtual QObject *create(QWebObjectPluginConnector *connector,
                            const QUrl &url,
                            const QString &mimeType,
                            const QStringList &argumentNames,
                            const QStringList &argumentValues) const = 0;
};

#endif

#endif
