/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginRepresentation.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebError.h>

@implementation WebNetscapePluginRepresentation

- (void)dealloc
{
    [_dataSource release];
    [_error release];
    [super dealloc];
}

- (void)setDataSource:(WebDataSource *)ds
{
    [ds retain];
    [_dataSource release];
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
    WebNetscapePluginDocumentView *view = (WebNetscapePluginDocumentView *)[[[_dataSource webFrame] frameView] documentView];
    ASSERT([view isKindOfClass:[WebNetscapePluginDocumentView class]]);
    
    if (![view isStarted]) {
        return;
    }
    
    if(!instance){
        [self setPluginPointer:[view pluginPointer]];
        [self setResponse:[ds response]];
    }
    
    ASSERT(instance);
    [self receivedData:data];
}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)ds
{
    [error retain];
    [_error release];
    _error = error;
    
    if (![self isPluginViewStarted]) {
        return;
    }
    
    if ([error errorCode] == WebFoundationErrorCancelled) {
        [self receivedError:NPRES_USER_BREAK];
    } else {
        [self receivedError:NPRES_NETWORK_ERR];
    }
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

- (void)redeliverStream
{
    if (_dataSource && [self isPluginViewStarted]) {
        instance = NULL;
        NSData *data = [_dataSource data];
        if ([data length] > 0) {
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
