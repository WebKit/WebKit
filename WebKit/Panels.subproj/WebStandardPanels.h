/*	
    WebStandardPanels.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebStandardPanelsPrivate;

@interface WebStandardPanels : NSObject
{
@private
    WebStandardPanelsPrivate *_privatePanels;
}

+(WebStandardPanels *)sharedStandardPanels;

-(void)setUseStandardAuthenticationPanel:(BOOL)use;
-(BOOL)useStandardAuthenticationPanel;
-(void)setUseStandardCookieAcceptPanel:(BOOL)use;
-(BOOL)useStandardCookieAcceptPanel;

-(void)didStartLoadingURL:(NSURL *)url inWindow:(NSWindow *)window;
-(void)didStopLoadingURL:(NSURL *)url inWindow:(NSWindow *)window;

-(NSWindow *)frontmostWindowLoadingURL:(NSURL *)url;

@end
