/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebBasePluginPackage;

@interface WebPluginDatabase : NSObject
{
    NSMutableSet *plugins;
    NSMutableSet *pendingPluginLoads;
}

+ (WebPluginDatabase *)installedPlugins;

// Plug-ins are returned in this order: New plug-in (WBPL), Mach-O Netscape, CFM Netscape
- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)mimeType;
- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension;

- (NSArray *)plugins;

- (void)refresh;

- (void)loadPluginIfNeededForMIMEType:(NSString *)MIMEType;

@end
