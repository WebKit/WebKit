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
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>


#define WebIconLoaderWeeksWorthOfSeconds (60 * 60 * 24 * 7)

@interface WebIconLoaderPrivate : NSObject
{
@public
    NSURLConnection *handle;
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
    
    // send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    NSURLRequest *request = [NSURLRequest requestWithURL:_private->URL];
    request = [self connection:nil willSendRequest:request redirectResponse:nil];
    _private->handle = [[NSURLConnection alloc] initWithRequest:request delegate:self];
}

- (void)stopLoading
{
    [_private->handle cancel];
    [_private->handle release];
    _private->handle = nil;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    NSImage *icon = [[NSImage alloc] initWithData:_private->resourceData];
    if (icon) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:[_private->URL absoluteString]];
    } else {
	[[WebIconDatabase sharedIconDatabase] _setHaveNoIconForIconURL:[_private->URL absoluteString]];
    }
    [_private->delegate _iconLoaderReceivedPageIcon:self];
    [icon release];
}

- (NSURLRequest *)connection:(NSURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
    return request;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)theResponse
{
    // no-op

    // no-op
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [_private->resourceData appendData:data];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)result
{

}

@end
