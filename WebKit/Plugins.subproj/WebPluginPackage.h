//
//  WebPluginPackage.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebKit/WebBasePluginPackage.h>

@protocol WebPluginViewFactory;

@interface WebPluginPackage : WebBasePluginPackage
{
}

- (Class)viewFactory;

@end
