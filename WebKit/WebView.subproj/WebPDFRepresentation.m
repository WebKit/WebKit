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

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPDFRepresentation.h>
#import <WebKit/WebPDFView.h>

#import <PDFKit/PDFDocument.h>
#import <PDFKit/PDFView.h>

@implementation WebPDFRepresentation

+ (Class)PDFDocumentClass
{
    static Class PDFDocumentClass = nil;
    if (PDFDocumentClass == nil) {
        PDFDocumentClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFDocument"];
        if (PDFDocumentClass == nil) {
            ERROR("Couldn't find PDFDocument class in PDFKit.framework");
        }
    }
    return PDFDocumentClass;
}

- (void)setDataSource:(WebDataSource *)dataSource;
{
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
{
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource;
{
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    WebPDFView *view = (WebPDFView *)[[[dataSource webFrame] frameView] documentView];
    PDFDocument *doc = [[[[self class] PDFDocumentClass] alloc] initWithData:[dataSource data]];
    [[view PDFSubview] setDocument:doc];
    [doc release];
}


- (BOOL)canProvideDocumentSource
{
    return NO;
}


- (NSString *)documentSource
{
    return nil;
}


- (NSString *)title
{
    return nil;
}

@end
