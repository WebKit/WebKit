//
//  WebIconLoader.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebIconLoader.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebNSURLExtras.h>

@interface WebIconLoaderPrivate : NSObject
{
@public
    id delegate;
    NSURLRequest *initialRequest;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [initialRequest release];
    [super dealloc];
}

@end;

@implementation WebIconLoader

- (id)initWithRequest:(NSURLRequest *)initialRequest;
{
    ASSERT([[WebIconDatabase sharedIconDatabase] _isEnabled]);
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->initialRequest = [initialRequest copy];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSURL *)URL
{
    return [_private->initialRequest URL];
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
    [self loadWithRequest:_private->initialRequest];
}

- (void)stopLoading
{
    [self cancel];
}

- (void)didFinishLoading
{
    NSImage *icon;
    NS_DURING
        NSData *data = [self resourceData];
        icon = [data length] > 0 ? [[NSImage alloc] initWithData:data] : nil;
    NS_HANDLER
        icon = nil;
    NS_ENDHANDLER
    if ([[icon representations] count] > 0) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:[[self URL] _web_originalDataAsString]];
    } else {
        [[WebIconDatabase sharedIconDatabase] _setHaveNoIconForIconURL:[[self URL] _web_originalDataAsString]];
    }
    [_private->delegate _iconLoaderReceivedPageIcon:self];    
    [icon release];
    
    [super didFinishLoading];
}

// We don't ever want to prompt for authentication just for a site icon, so
// override this WebBaseResourceDelegate method to refuse the challenge
- (void)didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    [[challenge sender] cancelAuthenticationChallenge:challenge];
}

- (void)didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
}

@end
