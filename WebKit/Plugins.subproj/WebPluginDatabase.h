/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebNetscapePluginPackage;

@interface WebPluginDatabase : NSObject
{
    NSArray *plugins;
}

+ (WebPluginDatabase *)installedPlugins;
- (WebNetscapePluginPackage *)pluginForMIMEType:(NSString *)mimeType;
- (WebNetscapePluginPackage *)pluginForExtension:(NSString *)extension;
- (WebNetscapePluginPackage *)pluginForFilename:(NSString *)filename;
- (NSArray *)plugins;

@end
