/*
        WebPluginDatabase.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebNetscapePlugin;

@interface WebNetscapePluginDatabase : NSObject
{
    NSArray *plugins;
}

+ (WebNetscapePluginDatabase *)installedPlugins;
- (WebNetscapePlugin *)pluginForMIMEType:(NSString *)mimeType;
- (WebNetscapePlugin *)pluginForExtension:(NSString *)extension;
- (WebNetscapePlugin *)pluginForFilename:(NSString *)filename;
- (NSArray *)plugins;

@end
