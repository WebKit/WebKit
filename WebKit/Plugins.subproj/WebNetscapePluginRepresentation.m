/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginRepresentation.h>
#import <WebKit/WebView.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebNetscapePluginRepresentation

- (void)setDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    if(!instance){
        [self setPluginPointer:[(WebNetscapePluginDocumentView *)[[[dataSource webFrame] webView] documentView] pluginPointer]];
        [self setResponse:[dataSource response]];
    }
    [self receivedData:data];
}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource
{
    if([error errorCode] == WebErrorCodeCancelled){
        [self receivedError:NPRES_USER_BREAK];
    } else {
        [self receivedError:NPRES_NETWORK_ERR];
    }
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    [self finishedLoadingWithData:[dataSource data]];
}

@end
