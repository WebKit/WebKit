/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebBasePluginPackage;

@interface WebPluginDatabase : NSObject
{
    NSArray *plugins;
}

+ (WebPluginDatabase *)installedPlugins;

// Plug-ins are returned in this order: New plug-in (WBPL), Mach-O Netscape, CFM Netscape
- (WebBasePluginPackage *)pluginForMIMEType:(NSString *)mimeType;
- (WebBasePluginPackage *)pluginForExtension:(NSString *)extension;
- (WebBasePluginPackage *)pluginForFilename:(NSString *)filename;

- (NSArray *)plugins;

@end
