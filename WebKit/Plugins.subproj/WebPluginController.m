//
//  WebPluginController.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebResourceRequest.h>

@implementation WebPluginController

- initWithWebFrame:(WebFrame *)theFrame
{
    // Not retained because the frame retains this plug-in controller.
    frame = theFrame;

    return self;
}

- (void)showURL:(NSURL *)URL inFrame:(NSString *)target
{
    if(!URL || !target){
        return;
    }

    WebFrame *otherFrame = [frame frameNamed:target];
    if(!otherFrame){
        // FIXME: Open new window instead of return.
        return;
    }

    WebDataSource *dataSource = [[WebDataSource alloc] initWithRequest:[WebResourceRequest requestWithURL:URL]];
    if(!dataSource){
        return;
    }

    if([otherFrame setProvisionalDataSource:dataSource]){
        [otherFrame startLoading];
    }
}

- (void)showStatus:(NSString *)message
{
    if(!message){
        return;
    }
    
    [[[frame controller] windowOperationsDelegate] setStatusText:message];
}

@end
