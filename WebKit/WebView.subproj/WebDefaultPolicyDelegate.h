/*	
        WebDefaultPolicyDelegate.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/

@class WebController;

@interface WebDefaultPolicyDelegate : NSObject <WebControllerPolicyDelegate>
{
    WebController *webController;
}
- initWithWebController: (WebController *)wc;
@end

