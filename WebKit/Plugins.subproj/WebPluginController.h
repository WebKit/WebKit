//
//  WebPluginController.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebKit/WebPluginContainer.h>

@class WebFrame;

@protocol WebPlugin;

@interface WebPluginController : NSObject <WebPluginContainer>
{
    WebFrame *frame;

    NSMutableArray *views;
}

- initWithWebFrame:(WebFrame *)theFrame;
- (void)addPluginView:(NSView <WebPlugin> *)view;
- (void)didAddPluginView:(NSView <WebPlugin> *)view;
- (void)destroyAllPlugins;

@end
