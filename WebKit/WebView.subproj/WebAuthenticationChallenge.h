/*	
    WebAuthenticationChallenge.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <WebFoundation/NSURLAuthenticationChallenge.h>

@class WebAuthenticationChallengeInternal;

/*!
    @class WebAuthenticationChallenge
    @discussion This class represents an authentication challenge
    issued by WebKit.
*/
@interface WebAuthenticationChallenge : NSURLAuthenticationChallenge
{
@private
    WebAuthenticationChallengeInternal *_webInternal;
}

/*!
    @method useCredential:
    @abstract Continue using the provided credential.
    @param credential The credential to use.
*/
- (void)useCredential:(NSURLCredential *)credential;

/*!
    @method cancel
    @abstract Cancel the load that caused the challenge.
*/
- (void)cancel;

/*!
    @method continueWithoutCredential
    @abstract Continue loading without providing a credential.
*/
- (void)continueWithoutCredential;

@end
