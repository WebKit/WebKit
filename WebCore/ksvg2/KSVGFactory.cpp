/*
    Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2005 Rob Buis <buis@kde.org>

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

#include <assert.h>

#include <qregexp.h>

#include <kdebug.h>
#include <kaboutdata.h>

#include <kdom/cache/KDOMCache.h>

#include "KSVGPart.h"
#include "KSVGFactory.h"
#include "KSVGSettings.h"
#include "SVGRenderStyle.h"
#include "SVGCSSStyleSelector.h"

using namespace KSVG;

extern "C" void *init_libksvg2()
{
    // See comment in init_libkhtml2()
    return new KSVGFactory(true);
}

KSVGFactory *KSVGFactory::s_self = 0;
unsigned long int KSVGFactory::s_refcnt = 0;
KInstance *KSVGFactory::s_instance = 0;
KAboutData *KSVGFactory::s_about = 0;
KSVGSettings *KSVGFactory::s_settings = 0;
QPtrList<KSVGPart> *KSVGFactory::s_parts = 0;

KSVGFactory::KSVGFactory(bool clone)
{
    if(clone)
        ref();
}

KSVGFactory::~KSVGFactory()
{
    if(s_self == this)
    {
        assert(!s_refcnt);

        delete s_instance;
        delete s_about;
        delete s_settings;
        
        if(s_parts)
        {
            assert(s_parts->isEmpty());
            delete s_parts;
        }

        s_instance = 0;
        s_about = 0;
        s_settings = 0;
        s_parts = 0;

        // clean up static data
        SVGCSSStyleSelector::clear();
        SVGRenderStyle::cleanup();
        KDOM::Cache::clear();
    }
    else
        deref();
}

KParts::Part *KSVGFactory::createPartObject(QWidget *parentWidget, const char *widgetName, QObject *parent, const char *name, const char *, const QStringList &args)
{
    // Get the width and height of the <embed> (<object> is TODO)
    unsigned int width = 0, height = 0;
    
    bool dummy;
    QRegExp r1(QString::fromLatin1("(WIDTH)(\\s*=\\s*\")(\\d+)(\\w*)(\")"));
    QRegExp r2(QString::fromLatin1("(HEIGHT)(\\s*=\\s*\")(\\d+)(\\w*)(\")"));
    for(QValueListConstIterator<QString> it = args.begin(); it != args.end(); ++it) 
    {
        if(r1.search(*it) > -1)
            width = r1.cap(3).toUInt(&dummy);
        if(r2.search(*it) > -1)
            height = r2.cap(3).toUInt(&dummy);
    }
    
    return new KSVGPart(parentWidget, widgetName, parent, name, width, height);
}

void KSVGFactory::ref()
{
    if(!s_refcnt && !s_self)
    {
        // we can't use a staticdeleter here, because that would mean
        // that the factory gets deleted from within a qPostRoutine, called
        // from the QApplication destructor. That however is too late, because
        // we want to destruct a KInstance object, which involves destructing
        // a KConfig object, which might call KGlobal::dirs() (in sync()) which
        // probably is not going to work ;-)
        // well, perhaps I'm wrong here, but as I'm unsure I try to stay on the
        // safe side ;-) -> let's use a simple reference counting scheme
        // (Simon)
        s_self = new KSVGFactory();
        KDOM::Cache::init();
    }

    s_refcnt++;
}

void KSVGFactory::deref()
{
    if(!--s_refcnt && s_self)
    {
        delete s_self;
        s_self = 0;
    }
}

void KSVGFactory::registerPart(KSVGPart *part)
{
    if(!s_parts)
        s_parts = new QPtrList<KSVGPart>();

    if(!s_parts->containsRef(part))
    {
        s_parts->append(part);
        ref();
    }
}

void KSVGFactory::deregisterPart(KSVGPart *part)
{
    assert(s_parts);

    if(s_parts->removeRef(part))
    {
        if(s_parts->isEmpty())
        {
            delete s_parts;
            s_parts = 0;
        }
        
        deref();
    }
}

KInstance *KSVGFactory::instance()
{
    assert(s_self);

    if(!s_instance)
    {
        s_about = new KAboutData("ksvg2", I18N_NOOP("KSVG"), "4.0",
                                 I18N_NOOP( "Embeddable SVG component" ),
                                 KAboutData::License_LGPL,
                                 "(c) 2001 - 2005, The KSVG Team", 0,
                                 "http://svg.kde.org");
    
        s_about->addAuthor("Rob Buis", 0, "buis@kde.org");
        s_about->addAuthor("Nikolas Zimmermann", 0, "wildfox@kde.org");
        
        s_about->addCredit("Adrian Page", 0);
        s_about->addCredit("Jim Ley", 0, "jim@jibbering.com");
        s_about->addCredit("Andreas Streichardt", 0, "mop@spaceregents.de");

        s_instance = new KInstance(s_about);
    }

    return s_instance;
}

KSVGSettings *KSVGFactory::defaultSVGSettings()
{
    assert(s_self);
    if(!s_settings)
        s_settings = new KSVGSettings();

    return s_settings;
}

// vim:ts=4:noet

