/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQExceptions.h"
#import "KWQKJavaAppletContext.h"
#import "WebCoreBridge.h"

KJavaAppletWidget::KJavaAppletWidget(const QSize &size, KJavaAppletContext *c, const QMap<QString, QString> &args)
{
    KWQ_BLOCK_EXCEPTIONS;
    
    NSMutableDictionary *attributes = [[NSMutableDictionary alloc] init];
    QMapConstIterator<QString, QString> it = args.begin();
    QMapConstIterator<QString, QString> end = args.end();
    QString baseURLString;
    while (it != end) {
        if (it.key().lower() == "baseurl") {
            baseURLString = it.data();
        }
        [attributes setObject:it.data().getNSString() forKey:it.key().getNSString()];
        ++it;
    }
    
    KHTMLPart *part = c->part();
    KURL baseURL = baseURLString.isNull() ? part->baseURL() : KURL(baseURLString);
    
    setView([KWQ(part)->bridge() viewForJavaAppletWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                                                 attributes:attributes
                                                    baseURL:baseURL.getNSURL()]);
    [attributes release];
    part->view()->addChild(this);
    
    KWQ_UNBLOCK_EXCEPTIONS;
}
