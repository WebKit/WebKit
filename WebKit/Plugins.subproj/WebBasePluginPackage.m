//
//  WebBasePluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginPackage.h>

@implementation WebBasePluginPackage

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath
{
    WebBasePluginPackage *pluginPackage = [[WebPluginPackage alloc] initWithPath:pluginPath];

    if(!pluginPackage){
        pluginPackage = [[WebNetscapePluginPackage alloc] initWithPath:pluginPath];
    }

    return pluginPackage;
}

- initWithPath:(NSString *)pluginPath
{
    [super init];
    return self;
}

- (NSString *)name{
    return name;
}

- (NSString *)path{
    return path;
}

- (NSString *)filename{
    return filename;
}

- (NSString *)pluginDescription
{
    return pluginDescription;
}

- (NSDictionary *)extensionToMIMEDictionary
{
    return extensionToMIME;
}

- (NSDictionary *)MIMEToExtensionsDictionary
{
    return MIMEToExtensions;
}

- (NSDictionary *)MIMEToDescriptionDictionary
{
    return MIMEToDescription;
}

- (BOOL)load
{
    // Overriden by subclasses.
    return NO;
}

- (void)unload
{
    // Overriden by subclasses.
}

- (BOOL)isLoaded
{
    return isLoaded;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"name: %@\npath: %@\nisLoaded: %d\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, isLoaded, [MIMEToExtensions description], [MIMEToDescription description], pluginDescription];
}

@end
