/*	
    WebAuthenticationChallenge.m
    Copyright 2003 Apple, Inc. All rights reserved.
*/


#import <WebKit/WebAuthenticationChallenge.h>
#import <WebKit/WebAuthenticationChallengeInternal.h>
#import <WebKit/WebBaseResourceHandleDelegate.h>

@interface WebAuthenticationChallengeInternal : NSObject
{
@public
    WebBaseResourceHandleDelegate *delegate;
}

- (id)initWithDelegate:(WebBaseResourceHandleDelegate *)d;
@end

@implementation WebAuthenticationChallengeInternal

- (id)initWithDelegate:(WebBaseResourceHandleDelegate *)d
{
    self = [super init];
    if (self != nil) {
	delegate = [d retain];
    }
    return self;
}

- (void)dealloc
{
    [delegate release];
    [super dealloc];
}

@end


@implementation WebAuthenticationChallenge (Internal)

- (id)_initWithAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge delegate:(WebBaseResourceHandleDelegate *)delegate
{
    self = [super initWithAuthenticationChallenge:challenge];
    if (self != nil) {
	_webInternal = [[WebAuthenticationChallengeInternal alloc] initWithDelegate:delegate];
    }
    return self;
}

@end

@implementation WebAuthenticationChallenge

- (void)dealloc
{
    [_webInternal release];
    [super dealloc];
}

- (void)useCredential:(NSURLCredential *)credential
{
    [_webInternal->delegate useCredential:credential forAuthenticationChallenge:self];
}

- (void)cancel
{
    [_webInternal->delegate cancel];

}

- (void)continueWithoutCredential
{
    [_webInternal->delegate continueWithoutCredentialForAuthenticationChallenge:self];
}

@end
