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
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>


#define WebIconLoaderWeeksWorthOfSeconds (60 * 60 * 24 * 7)

@interface WebIconLoaderPrivate : NSObject
{
@public
    NSURLConnection *handle;
    id delegate;
    NSURLRequest *request;
    NSMutableData *resourceData;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [request release];
    [handle release];
    [resourceData release];
    [super dealloc];
}

@end;

@implementation WebIconLoader

- (id)initWithRequest:(NSURLRequest *)request;
{
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->request = [request retain];
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
    return [_private->request URL];
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

    _private->handle = [[NSURLConnection alloc] initWithRequest:_private->request delegate:self];
}

- (void)stopLoading
{
    [_private->handle cancel];
    [_private->handle release];
    _private->handle = nil;
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [_private->resourceData appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    NSImage *icon = [[NSImage alloc] initWithData:_private->resourceData];
    if (icon && [[icon representations] count]) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:[[self URL] _web_originalDataAsString]];
    } else {
	[[WebIconDatabase sharedIconDatabase] _setHaveNoIconForIconURL:[[self URL] _web_originalDataAsString]];
    }
    [_private->delegate _iconLoaderReceivedPageIcon:self];    
    [icon release];
}

@end
