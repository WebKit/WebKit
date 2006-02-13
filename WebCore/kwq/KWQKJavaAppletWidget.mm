/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "KWQKJavaAppletWidget.h"

#import "DocumentImpl.h"
#import "FrameView.h"
#import "KWQExceptions.h"
#import "MacFrame.h"
#import "WebCoreFrameBridge.h"

using namespace WebCore;

typedef HashMap<String, String> StringMap;

KJavaAppletWidget::KJavaAppletWidget(const IntSize& size, Frame* frame, const StringMap& args)
{
    KWQ_BLOCK_EXCEPTIONS;
    
    NSMutableArray *attributeNames = [[NSMutableArray alloc] init];
    NSMutableArray *attributeValues = [[NSMutableArray alloc] init];

    QString baseURLString;
    StringMap::const_iterator end = args.end();
    for (StringMap::const_iterator it = args.begin(); it != end; ++it) {
        if (it->first.lower() == "baseurl")
            baseURLString = it->second.qstring();
        [attributeNames addObject:it->first];
        [attributeValues addObject:it->second];
    }
    
    if (baseURLString.isEmpty()) {
        baseURLString = frame->document()->baseURL();
    }
    setView([Mac(frame)->bridge() viewForJavaAppletWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                                        attributeNames:attributeNames
                                       attributeValues:attributeValues
                                               baseURL:frame->completeURL(baseURLString).getNSURL()]);
    [attributeNames release];
    [attributeValues release];
    frame->view()->addChild(this);
    
    KWQ_UNBLOCK_EXCEPTIONS;
}
