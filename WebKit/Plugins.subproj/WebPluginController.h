//
//  WebPluginController.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebHTMLView;

@protocol WebPlugin;
@protocol WebPluginContainer;

@interface WebPluginController : NSObject <WebPluginContainer>
{
    WebHTMLView *_HTMLView;
    NSMutableArray *_views;
    BOOL _started;
}

- (id)initWithHTMLView:(WebHTMLView *)HTMLView;

- (void)addPlugin:(NSView <WebPlugin> *)view;

- (void)startAllPlugins;
- (void)stopAllPlugins;
- (void)destroyAllPlugins;

@end
