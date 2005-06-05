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

#import "WebImageRepresentation.h"

#import <WebKit/WebArchive.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebResource.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <Foundation/NSURLRequest.h>

@implementation WebImageRepresentation

- (void)dealloc
{
    [image release];
    [super dealloc];
}

- (WebImageRenderer *)image
{
    return image;
}

- (NSURL *)URL
{
    return [[dataSource request] URL];
}

- (BOOL)doneLoading
{
    return doneLoading;
}

- (void)setDataSource:(WebDataSource *)theDataSource
{
    dataSource = theDataSource;
    image = [[[WebImageRendererFactory sharedFactory] imageRendererWithMIMEType:[[dataSource response] MIMEType]] retain];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:NO callback:0];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    if ([allData length] > 0) {
        [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES callback:0];
    }
    doneLoading = YES;
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES callback:0];
    doneLoading = YES;
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
    NSString *filename = [self filename];
    NSSize size = [image size];
    if (!NSEqualSizes(size, NSZeroSize)) {
        return [NSString stringWithFormat:UI_STRING("%@ %.0f×%.0f pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filename, size.width, size.height];
    }
    return filename;
}

- (NSData *)data
{
    return [dataSource data];
}

- (NSString *)filename
{
    return [[dataSource response] suggestedFilename];
}

- (WebArchive *)archive
{
    return [dataSource webArchive];
}

@end
