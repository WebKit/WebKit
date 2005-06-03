/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
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
