/*	
        WebDefaultControllerPolicyHandler.h
	Copyright 2002, Apple Computer, Inc.

        Public header file.
*/

@class WebController;

@interface WebDefaultControllerPolicyHandler : NSObject <WebControllerPolicyHandler>
{
    WebController *webController;
}
- initWithWebController: (WebController *)wc;
@end

