/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Private header file.
*/
#import <Foundation/Foundation.h>

/*!
    @class WebDefaultResourceLoadDelegate
    @discussion WebDefaultPolicyDelegate will be used as a WebView's
    default policy delegate.  It can be subclassed to modify policies. 
*/
@interface WebDefaultResourceLoadDelegate : NSObject
+ (WebDefaultResourceLoadDelegate *)sharedResourceLoadDelegate;
@end

