/*	
        WebDefaultFrameLoadDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Private header file.
*/
#import <Foundation/Foundation.h>

/*!
    @class WebDefaultFrameLoadDelegate
    @discussion WebDefaultPolicyDelegate will be used as a WebView's
    default policy delegate.  It can be subclassed to modify policies. 
*/
@interface WebDefaultFrameLoadDelegate : NSObject
+ (WebDefaultFrameLoadDelegate *)sharedFrameLoadDelegate;
@end

