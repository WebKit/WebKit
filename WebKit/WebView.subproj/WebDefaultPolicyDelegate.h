/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/
#import <Foundation/Foundation.h>

@class WebController;
@class WebURLPolicy;

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
    @method defaultURLPolicyForURL:
    @abstract Provides the default WebURLPolicy for a URL
    @discussion WebControllerPolicyDelegates can use this method to
    implement the standard behavior for -URLPolicyForURL:.
    @param URL use this URL to determine an appropriate policy
    @result The WebURLPolicy to use for the URL.
*/    
+ (WebURLPolicy *)defaultURLPolicyForURL: (NSURL *)URL;


/*!
    @method initWithWebController:
    @param webController The controller that will use this delegate.  Note that the controller is not retained.
    @result An initialized WebDefaultPolicyDelegate
*/
- initWithWebController: (WebController *)webController;
@end

