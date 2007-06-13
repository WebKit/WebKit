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
#include "qwebobjectpluginconnector.h"
#include "qwebnetworkinterface.h"
#include "qwebnetworkinterface_p.h"
#include "qwebframe.h"
#include "qwebpage.h"

struct QWebObjectPluginConnectorPrivate
{
    QWebFrame *frame;
};

QWebObjectPluginConnector::QWebObjectPluginConnector(QWebFrame *frame)
{
    d = new QWebObjectPluginConnectorPrivate;
    d->frame = frame;
}

QWebFrame *QWebObjectPluginConnector::frame() const
{
    return d->frame;
}

QWidget *QWebObjectPluginConnector::pluginParentWidget() const
{
    return d->frame->viewport();
}

QWebNetworkJob *QWebObjectPluginConnector::requestUrl(const QWebNetworkRequest &request, Target target)
{
    if (target != Plugin)
        return 0;

    QWebNetworkJob *job = new QWebNetworkJob;
    QWebNetworkJobPrivate *p = job->d;
    p->interface = d->frame->page()->networkInterface();
    p->connector = this;

    p->request = *request.d;

    d->frame->page()->networkInterface()->addJob(job);
    return job;
}
