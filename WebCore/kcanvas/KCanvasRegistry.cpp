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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "KCanvasItem.h"
#include "KCanvasRegistry.h"
#include "KRenderingPaintServer.h"
#include <qtextstream.h>

KCanvasRegistry::KCanvasRegistry()
{
    m_pservers.setAutoDelete(true);
    m_resources.setAutoDelete(true);
}

KCanvasRegistry::~KCanvasRegistry()
{
}

void KCanvasRegistry::addPaintServerById(const QString &id, KRenderingPaintServer *pserver)
{
    m_pservers.insert(id, pserver);
    pserver->setIdInRegistry(id);
}

KRenderingPaintServer *KCanvasRegistry::getPaintServerById(const QString &id) const
{
    return m_pservers[id];
}

void KCanvasRegistry::addResourceById(const QString &id, KCanvasResource *resource)
{
    m_resources.insert(id, resource);
    resource->setIdInRegistry(id);
}

KCanvasResource *KCanvasRegistry::getResourceById(const QString &id) const
{
    return m_resources[id];
}

void KCanvasRegistry::cleanup()
{
    m_pservers.clear();
    m_resources.clear();
}

QTextStream &operator<<(QTextStream &ts, const KCanvasRegistry &r)
{
    ts << "KCanvasRegistry: ";
    if (r.m_resources.count() == 0 && r.m_pservers.count() == 0)
    {
        ts << "empty" << endl;
    }
    else
    {
        ts << endl;
        for (Q3DictIterator<KCanvasResource> it(r.m_resources); (*it); ++it)
        {
            ts << "  KCanvasResource {id=\"" << it.currentKey() << "\" " << *(*it) << "}" << endl;
        }
        for (Q3DictIterator<KRenderingPaintServer> it(r.m_pservers); (*it); ++it)
        {
            ts << "  KRenderingPaintServer {id=\"" << it.currentKey() << "\" " << *(*it) <<"}" << endl;
        }
    }

    return ts;
}

// vim:ts=4:noet
