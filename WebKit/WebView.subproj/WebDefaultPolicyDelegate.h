/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/
#import <Foundation/Foundation.h>

/*!
    @class WebDefaultPolicyDelegate
    @discussion WebDefaultPolicyDelegate will be used as a WebController's
    default policy delegate.  It can be subclassed to modify policies. 
*/
@interface WebDefaultPolicyDelegate : NSObject <WebControllerPolicyDelegate>
@end

