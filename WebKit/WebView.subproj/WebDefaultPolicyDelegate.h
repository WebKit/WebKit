/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/
#import <Foundation/Foundation.h>

@class WebController;

/*!
    @class WebDefaultPolicyDelegate
    @discussion WebDefaultPolicyDelegate will be used a a WebController's
    default policy delegate.  It can be subclassed to modify policies. 
*/
@interface WebDefaultPolicyDelegate : NSObject <WebControllerPolicyDelegate>
{
    WebController *webController;
}

/*!
    @method defaultURLPolicyForRequest:
    @abstract Provides the default WebURLPolicy for a Request
    @discussion WebControllerPolicyDelegates can use this method to
    implement the standard behavior for -URLPolicyForRequest:.
    @param Request use this request to determine an appropriate policy
    @result The WebURLPolicy to use for the request.
*/    
+ (WebURLAction)defaultURLPolicyForRequest:(WebResourceRequest *)request;


/*!
    @method initWithWebController:
    @param webController The controller that will use this delegate.  Note that the controller is not retained.
    @result An initialized WebDefaultPolicyDelegate
*/
- initWithWebController: (WebController *)webController;
@end

