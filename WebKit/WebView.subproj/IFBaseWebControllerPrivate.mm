/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebView.h>


@implementation IFBaseWebControllerPrivate

- init 
{
    mainFrame = nil;
    return self;
}

- (void)dealloc
{
    [mainFrame autorelease];
    [super dealloc];
}

@end
