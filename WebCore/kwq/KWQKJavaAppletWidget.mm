/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import <kjavaappletwidget.h>

#import <kjavaappletcontext.h>
#import <kurl.h>
#import <KWQKHTMLPartImpl.h>
#import <WebCoreBridge.h>

KJavaAppletWidget::KJavaAppletWidget(KJavaAppletContext *c, QWidget *)
    : m_applet(*this)
    , m_baseURL(nil)
    , m_parameters([[NSMutableDictionary alloc] init])
{
    m_context = c;
}

KJavaAppletWidget::~KJavaAppletWidget()
{
    [m_baseURL release];
    [m_parameters release];
}

void KJavaAppletWidget::setBaseURL(const QString &baseURL)
{
    [m_baseURL release];
    m_baseURL = [KURL(baseURL).getNSURL() retain];
}

void KJavaAppletWidget::setParameter(const QString &name, const QString &value)
{
    // When putting strings into dictionaries, we should use an immutable copy.
    // That's not necessary for keys, because they are copied.
    NSString *immutableString = [value.getNSString() copy];
    [m_parameters setObject:immutableString forKey:name.getNSString()];
    [immutableString release];
}

void KJavaAppletWidget::processArguments(const QMap<QString, QString> &arguments)
{
    for (QMap<QString, QString>::ConstIterator it = arguments.begin(); it != arguments.end(); ++it) {
        setParameter(it.key(), it.data());
    }
}

void KJavaAppletWidget::showApplet()
{
    setView([m_context->part()->impl->bridge()
viewForJavaAppletWithFrame:NSMakeRect(pos().x(), pos().y(), size().width(), size().height())
                   baseURL:m_baseURL
                parameters:m_parameters]);
}
