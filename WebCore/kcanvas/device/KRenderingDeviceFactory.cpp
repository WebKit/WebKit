/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
*/

#include <qfile.h>

#include <kdebug.h>
#include <ktrader.h>
#include <kservice.h>
#include <klibloader.h>
#include <kstaticdeleter.h>
#include <kparts/componentfactory.h>

#include "KRenderingDevice.h"
#include "KRenderingDeviceFactory.h"

static KStaticDeleter<KInstance> instanceDeleter;
KInstance *KRenderingDeviceFactory::s_instance = 0;

static KStaticDeleter<KRenderingDeviceFactory> parserFactoryDeleter;
KRenderingDeviceFactory *KRenderingDeviceFactory::s_factory = 0;

KRenderingDeviceFactory::KRenderingDeviceFactory()
{
}

KRenderingDeviceFactory::~KRenderingDeviceFactory()
{
}

KRenderingDeviceFactory *KRenderingDeviceFactory::self()
{
    if(!s_factory)
    {
        s_factory = parserFactoryDeleter.setObject(s_factory, new KRenderingDeviceFactory());
        s_instance = instanceDeleter.setObject(s_instance, new KInstance("KRenderingDeviceFactory"));
    }

    return s_factory;
}

KRenderingDevice *KRenderingDeviceFactory::request(const QString &device) const
{
    QValueList<KService::Ptr> traderList = KTrader::self()->query("KCanvas/Device", "(Type == 'Service')");
    KTrader::OfferList::Iterator it(traderList.begin());
    for( ; it != traderList.end(); ++it)
    {
        KService::Ptr ptr = (*it);

        QString name = ptr->property("Name").toString();
        QString internal = ptr->property("X-KCanvas-DeviceName").toString();
        if(name.isEmpty() || internal.isEmpty() || internal != device)
            continue;    

        QStringList args;
        KRenderingDevice *retVal = KParts::ComponentFactory::createInstanceFromLibrary<KRenderingDevice>(QFile::encodeName(ptr->library()), 0, 0, args);

        if(!retVal)
            kdError() << "[KRenderingDeviceFactory] Failed to load device: " << device << " (" << KLibLoader::self()->lastErrorMessage() << ") FATAL ERROR." << endl;
        
        return retVal;
    }

    kdError() << "[KRenderingDeviceFactory] Device \"" << device << "\" does not exist or can't be loaded!" << endl;
    return 0;
}

const QValueList<KRenderingDeviceInfo> KRenderingDeviceFactory::deviceList() const
{
    QValueList<KRenderingDeviceInfo> list;
    
    QValueList<KService::Ptr> traderList = KTrader::self()->query("KCanvas/Device", "(Type == 'Service')");
    KTrader::OfferList::Iterator it(traderList.begin());
    for( ; it != traderList.end(); ++it)
    {
        KService::Ptr ptr = (*it);

        QString name = ptr->property("Name").toString();
        QString internal = ptr->property("X-KCanvas-DeviceName").toString();
        if(name.isEmpty() || internal.isEmpty())
            continue;    
    
        KRenderingDeviceInfo info;
        info.name = name;
        info.internal = internal;

        list.append(info);
    }

    return list;
}

// vim:ts=4:noet
