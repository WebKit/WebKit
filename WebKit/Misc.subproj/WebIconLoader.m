//
//  WebIconLoader.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebIconLoader.h>

#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconDatabasePrivate.h>

#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>


#define WebIconLoaderWeeksWorthOfSeconds (60 * 60 * 24 * 7)

@interface WebIconLoaderPrivate : NSObject
{
@public
    WebResource *handle;
    id delegate;
    NSURL *URL;
    NSMutableData *resourceData;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [URL release];
    [handle release];
    [resourceData release];
    [super dealloc];
}

@end;

@implementation WebIconLoader

+ iconLoaderWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL] autorelease];
}

- initWithURL:(NSURL *)URL
{
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->URL = [URL retain];
    _private->resourceData = [[NSMutableData alloc] init];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSURL *)URL
{
    return _private->URL;
}

- (id)delegate
{
    return _private->delegate;
}

- (void)setDelegate:(id)delegate
{
    _private->delegate = delegate;
}

- (void)startLoading
{
    if (_private->handle != nil) {
        return;
    }
    
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:_private->URL];
    [request HTTPSetPageNotFoundCacheLifetime:WebIconLoaderWeeksWorthOfSeconds];
    _private->handle = [[WebResource alloc] initWithRequest:request];
    [_private->handle loadWithDelegate:self];
    [request release];
}

- (void)stopLoading
{
    [_private->handle cancel];
    [_private->handle release];
    _private->handle = nil;
}

- (void)resourceDidFinishLoading:(WebResource *)sender
{
    NSImage *icon = [[NSImage alloc] initWithData:_private->resourceData];
    if (icon) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:[_private->URL absoluteString]];
        [_private->delegate iconLoader:self receivedPageIcon:icon];
        [icon release];
    }
}

-(NSURLRequest *)resource:(WebResource *)resource willSendRequest:(NSURLRequest *)request
{
    return request;
}

-(void)resource:(WebResource *)resource didReceiveResponse:(NSURLResponse *)theResponse
{
    // no-op
}

- (void)resource:(WebResource *)sender didReceiveData:(NSData *)data
{
    [_private->resourceData appendData:data];
}

- (void)resource:(WebResource *)sender didFailLoadingWithError:(WebError *)result
{
}

@end
