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

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceLoadManager.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>

@interface WebIconLoaderPrivate : NSObject
{
@public
    WebResourceHandle *handle;
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

    NSImage *icon = [[WebIconDatabase sharedIconDatabase] _iconForIconURL:_private->URL];
    if (icon) {
        [_private->delegate iconLoader:self receivedPageIcon:icon];
        return;
    }
    
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:_private->URL];
    _private->handle = [[WebResourceHandle alloc] initWithRequest:request client:self];
    [request release];
    if (_private->handle) {
        [_private->handle loadInBackground];
    }
}

- (void)stopLoading
{
    [_private->handle cancelLoadInBackground];
    [_private->handle release];
    _private->handle = nil;
}

- (NSString *)handleWillUseUserAgent:(WebResourceHandle *)handle forURL:(NSURL *)URL
{
    return nil;
}

- (void)handleDidFinishLoading:(WebResourceHandle *)sender
{
    NSImage *icon = [[NSImage alloc] initWithData:_private->resourceData];
    if (icon) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:_private->URL];
        [_private->delegate iconLoader:self receivedPageIcon:icon];
        [icon release];
    }
}

- (void)handleDidReceiveData:(WebResourceHandle *)sender data:(NSData *)data
{
    [_private->resourceData appendData:data];
}

- (void)handleDidFailLoading:(WebResourceHandle *)sender withError:(WebError *)result
{
}

- (void)handleDidRedirect:(WebResourceHandle *)sender toURL:(NSURL *)URL
{
}

@end
