/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQKJavaAppletWidget.h"

#import "KHTMLView.h"
#import "KWQKJavaAppletContext.h"
#import "KWQKURL.h"
#import "KWQKHTMLPart.h"
#import "KWQView.h"
#import "WebCoreBridge.h"

KJavaAppletWidget::KJavaAppletWidget(KJavaAppletContext *c, QWidget *)
    : _applet(*this)
    , _context(c)
    , _parameters([[NSMutableDictionary alloc] init])
{
}

KJavaAppletWidget::~KJavaAppletWidget()
{
    [_parameters release];
}

void KJavaAppletWidget::setParameter(const QString &name, const QString &value)
{
    // When putting strings into dictionaries, we should use an immutable copy.
    // That's not necessary for keys, because they are copied.
    NSString *immutableString = [value.getNSString() copy];
    [_parameters setObject:immutableString forKey:name.lower().getNSString()];
    [immutableString release];
}

void KJavaAppletWidget::showApplet()
{
    // If the view is a KWQView, we haven't replaced it with the Java view yet.
    // Only set the Java view once.
    if ([getView() isKindOfClass:[KWQView class]]) {
        setView([KWQ(_context->part())->bridge()
            viewForJavaAppletWithFrame:NSMakeRect(x(), y(), width(), height())
                            attributes:_parameters
                               baseURL:KURL(_baseURL).getNSURL()]);
        // Add the view to the main view now so the applet starts immediately rather than until the first paint.
        _context->part()->view()->addChild(this, x(), y());
    }
}
