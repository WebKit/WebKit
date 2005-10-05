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

#include "config.h"
#include <kglobal.h>
#include <kconfig.h>

#include "KSVGSettings.h"

using namespace KSVG;

class KSVGSettings::Private
{
public:
    QString renderingBackend; // Default: 'agg'
    QString parsingBackend; // Default: 'libxml'

    bool fontKerning : 1; // Default: 'true'
    bool progressive : 1; // Default: 'true'
};

KSVGSettings::KSVGSettings() : KDOM::KDOMSettings(), d(new Private())
{
    init();
}

KSVGSettings::KSVGSettings(const KSVGSettings &other) : d(new Private())
{
    *d = *other.d;
}

KSVGSettings::~KSVGSettings()
{
    delete d;
}

void KSVGSettings::init()
{
    KConfig global(QString::fromLatin1("ksvgrc"), true, false);
    init(&global, true);

    KConfig *local = KGlobal::config();
    if(!local)
        return;

    init(local, false);
}

void KSVGSettings::init(KConfig *config, bool reset)
{
    QString group_save = config->group();
    
    if(reset || config->hasGroup("Rendering"))
    {
        config->setGroup("Rendering");

        if(reset || config->hasKey("Backend"))
            d->renderingBackend = config->readEntry("Backend", QString::fromLatin1("agg"));

        if(reset || config->hasKey("FontKerning"))
            d->fontKerning = config->readBoolEntry("FontKerning", true);

        if(reset || config->hasKey("ProgressiveRendering"))
            d->progressive = config->readBoolEntry("ProgressiveRendering", true);
    }

    if(reset || config->hasGroup("Parsing"))
    {
        config->setGroup("Parsing");

        if(reset || config->hasKey("Backend"))
            d->parsingBackend = config->readEntry("Backend", QString::fromLatin1("libxml"));
    }

    config->setGroup(group_save);
}

QString KSVGSettings::parsingBackend() const
{
    return d->parsingBackend;
}

void KSVGSettings::setParsingBackend(const QString &backend) const
{
    d->parsingBackend = backend;

    KConfig *config = KGlobal::config();
    config->setGroup("Parsing");
    config->writeEntry("Backend", backend);
    config->sync();
}

QString KSVGSettings::renderingBackend() const
{
    return d->renderingBackend;
}

void KSVGSettings::setRenderingBackend(const QString &backend) const
{
    d->renderingBackend = backend;

    KConfig *config = KGlobal::config();
    config->setGroup("Rendering");
    config->writeEntry("Backend", backend);
    config->sync();
}

bool KSVGSettings::fontKerning() const
{
    return d->fontKerning;
}

void KSVGSettings::setFontKerning(bool value)
{
    d->fontKerning = value;

    KConfig *config = KGlobal::config();
    config->setGroup("Rendering");
    config->writeEntry("FontKerning", value);
    config->sync();
}

bool KSVGSettings::progressiveRendering() const
{
    return d->progressive;
}
        
void KSVGSettings::setProgressiveRendering(bool value)
{
    d->progressive = value;

    KConfig *config = KGlobal::config();
    config->setGroup("Rendering");
    config->writeEntry("ProgressiveRendering", value);
    config->sync();
}

// vim:ts=4:noet
