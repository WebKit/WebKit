/*	
    WebStandardPanelsPrivate.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebStandardPanels.h>
#import <WebKit/WebController.h>

@interface WebStandardPanels (Private)

-(void)_didStartLoadingURL:(NSURL *)URL inController:(WebView *)controller;
-(void)_didStopLoadingURL:(NSURL *)URL inController:(WebView *)controller;

@end
