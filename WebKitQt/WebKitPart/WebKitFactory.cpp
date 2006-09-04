/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitFactory.h"

#include "WebKitPart.h"

#include <klocale.h>
#include <kinstance.h>
#include <kaboutdata.h>

WebKitFactory* WebKitFactory::s_self = 0;
unsigned long int WebKitFactory::s_refCount = 0;
KInstance* WebKitFactory::s_instance = 0;
KAboutData* WebKitFactory::s_about = 0;

WebKitFactory::WebKitFactory(bool clone)
{
    if (clone)
        ref();
}

WebKitFactory::~WebKitFactory()
{
    if (s_self == this) {
        Q_ASSERT(!s_refCount);

        delete s_instance;
        delete s_about;
        
        s_instance = 0;
        s_about = 0;
    }
    else
        deref();
}

KParts::Part* WebKitFactory::createPartObject(QWidget* parentWidget,
                                              QObject* parentObject,
                                              const char* className,
                                              const QStringList& args)
{
    WebKitPart::GUIProfile prof = WebKitPart::DefaultGUI;

    if (strcmp(className, "Browser/View") == 0)
        prof = WebKitPart::BrowserViewGUI;

    return new WebKitPart(parentWidget, parentObject, prof);
}

KInstance* WebKitFactory::instance()
{
    Q_ASSERT(s_self != 0);

    if (!s_instance) {
        s_about = new KAboutData("WebKitPart", I18N_NOOP("WebKit"), "0.1",
                                 I18N_NOOP("Embeddable HTML/SVG component"),
                                 KAboutData::License_LGPL);

        // FIXME: Add copyright information in the KAboutData!
        s_instance = new KInstance(s_about);
    }

    return s_instance;
}

void WebKitFactory::ref()
{
    if (!s_refCount && !s_self)
        s_self = new WebKitFactory();

    s_refCount++;
}

void WebKitFactory::deref()
{
    if(!--s_refCount && s_self) {
        delete s_self;
        s_self = 0;
    }
}

// Factory entry point
extern "C" void* init_libWebKitPart()
{        
    return new WebKitFactory(true);
}

#include "WebKitFactory.moc"
