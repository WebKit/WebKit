/*
     WebDefaultResourceLoadDelegate.m
     Copyright 2003, Apple, Inc. All rights reserved.
*/

#import <WebFoundation/WebRequest.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/WebResponse.h>
#import <WebFoundation/WebError.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebPluginError.h>


@implementation WebDefaultResourceLoadDelegate

static WebDefaultResourceLoadDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultResourceLoadDelegate *)sharedResourceLoadDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultResourceLoadDelegate alloc] init];
    }
    return sharedDelegate;
}

- identifierForInitialRequest: (WebRequest *)request fromDataSource: (WebDataSource *)dataSource
{
    return [[[NSObject alloc] init] autorelease];
}

-(WebRequest *)resource:identifier willSendRequest: (WebRequest *)newRequest fromDataSource:(WebDataSource *)dataSource
{
    return newRequest;
}

-(void)resource:identifier didReceiveResponse: (WebResponse *)response fromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource
{
}

-(void)resource:identifier didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource
{
}

- (void)pluginFailedWithError:(WebPluginError *)error dataSource:(WebDataSource *)dataSource
{
}

@end


