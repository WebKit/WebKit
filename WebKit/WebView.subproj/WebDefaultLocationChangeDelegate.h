/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Private header file.
*/
#import <Foundation/Foundation.h>

/*!
    @class WebDefaultLocationChangeDelegate
    @discussion WebDefaultPolicyDelegate will be used as a WebController's
    default policy delegate.  It can be subclassed to modify policies. 
*/
@interface WebDefaultLocationChangeDelegate : NSObject
+ (WebDefaultLocationChangeDelegate *)sharedLocationChangeDelegate;
@end

