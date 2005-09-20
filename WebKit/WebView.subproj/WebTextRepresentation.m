/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebTextRepresentation.h"

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebTextView.h>

#import <Foundation/NSURLResponse.h>
#import <WebKit/WebAssertions.h>

@implementation WebTextRepresentation

+ (NSArray *)supportedMIMETypes
{
    return [NSArray arrayWithObjects:
            @"text/",
            @"application/x-javascript",
            nil];
}

- (void)dealloc
{
    [RTFSource release];
    [super dealloc];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    // FIXME: This is broken. [dataSource data] is incomplete at this point.
    hasRTFSource = [[[dataSource response] MIMEType] isEqualToString:@"text/rtf"];
    if (hasRTFSource){
        RTFSource = [[dataSource _stringWithData: [dataSource data]] retain];
    }
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    WebTextView *view = (WebTextView *)[[[dataSource webFrame] frameView] documentView];
    ASSERT([view isKindOfClass:[WebTextView class]]);
    [view appendReceivedData:data fromDataSource:dataSource];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{

}

- (BOOL)canProvideDocumentSource
{
    return hasRTFSource;
}

- (NSString *)documentSource
{
    return RTFSource;
}

- (NSString *)title
{
    return nil;
}


@end
