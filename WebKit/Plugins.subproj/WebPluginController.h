//
//  WebPluginController.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebDataSource;

@protocol WebPlugin;
@protocol WebPluginContainer;

@interface WebPluginController : NSObject <WebPluginContainer>
{
    WebDataSource *_dataSource;
    NSMutableArray *_views;
    BOOL _started;
}

- (id)initWithDataSource:(WebDataSource *)dataSource;
- (void)dataSourceWillBeDeallocated;

- (void)addPlugin:(NSView <WebPlugin> *)view;

- (void)startAllPlugins;
- (void)stopAllPlugins;

@end
