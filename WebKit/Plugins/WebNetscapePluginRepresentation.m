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

#import <WebKit/WebNetscapePluginRepresentation.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginPackage.h>

#import <Foundation/NSURLResponse.h>

@implementation WebNetscapePluginRepresentation

- (void)dealloc
{
    [_error release];
    [super dealloc];
}

- (void)setDataSource:(WebDataSource *)ds
{
    _dataSource = ds;
}

- (BOOL)isPluginViewStarted
{
    WebNetscapePluginDocumentView *view = (WebNetscapePluginDocumentView *)[[[_dataSource webFrame] frameView] documentView];
    ASSERT([view isKindOfClass:[WebNetscapePluginDocumentView class]]);
    return [view isStarted];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)ds
{
    _dataLengthReceived += [data length];
    
    WebNetscapePluginDocumentView *view = (WebNetscapePluginDocumentView *)[[[_dataSource webFrame] frameView] documentView];
    ASSERT([view isKindOfClass:[WebNetscapePluginDocumentView class]]);
    
    if (![view isStarted]) {
        return;
    }
    
    if (instance == NULL) {
        [self setRequestURL:[[_dataSource request] URL]];
        [self setPluginPointer:[view pluginPointer]];
        [self startStreamWithResponse:[ds response]];
    }
    
    ASSERT(instance != NULL);
    [self receivedData:data];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)ds
{
    [error retain];
    [_error release];
    _error = error;
    
    if (![self isPluginViewStarted]) {
        return;
    }
    
    [self destroyStreamWithError:error];
}

- (void)cancelLoadWithError:(NSError *)error
{
    [_dataSource _stopLoadingWithError:error];
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)ds
{    
    if ([self isPluginViewStarted]) {
        [self finishedLoadingWithData:[ds data]];
    }
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

- (void)redeliverStream
{
    if (_dataSource && [self isPluginViewStarted]) {
        // Deliver what has not been passed to the plug-in up to this point.
        if (_dataLengthReceived > 0) {
            NSData *data = [[_dataSource data] subdataWithRange:NSMakeRange(0, _dataLengthReceived)];
            instance = NULL;
            _dataLengthReceived = 0;
            [self receivedData:data withDataSource:_dataSource];
            if (![_dataSource isLoading]) {
                if (_error) {
                    [self receivedError:_error withDataSource:_dataSource];
                } else {
                    [self finishedLoadingWithDataSource:_dataSource];
                }
            }
        }
    }
}

@end
