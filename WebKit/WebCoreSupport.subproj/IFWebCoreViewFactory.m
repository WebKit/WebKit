//
//  IFWebCoreViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFWebCoreViewFactory.h"
#import "IFPluginDatabase.h"
#import "IFPluginView.h"
#import "IFNullPluginView.h"
#import "WebKitDebug.h"

@implementation IFWebCoreViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    WEBKIT_ASSERT([[self sharedFactory] isMemberOfClass:self]);
}

- (NSView *)viewForPluginWithURL:(NSString *)pluginURL serviceType:(NSString *)serviceType arguments:(NSArray *)args baseURL:(NSString *)baseURL
{
    NSMutableDictionary *arguments;
    NSString *mimeType;
    NSRange r1, r2, r3;
    IFPlugin *plugin;
    uint i;
    
    arguments = [NSMutableDictionary dictionary];
    for (i = 0; i < [args count]; i++){
        NSString *arg = [args objectAtIndex:i];
        if ([arg rangeOfString:@"__KHTML__"].length == 0) {
            r1 = [arg rangeOfString:@"="]; // parse out attributes and values
            r2 = [arg rangeOfString:@"\""];
            r3.location = r2.location + 1;
            r3.length = [arg length] - r2.location - 2; // don't include quotes
            [arguments setObject:[arg substringWithRange:r3] forKey:[arg substringToIndex:r1.location]];
        }
    }
    
    if ([baseURL length]) {
        [arguments setObject:baseURL forKey:@"WebKitBaseURL"];
    }
        
    if ([serviceType length]) {
        mimeType = serviceType;
        plugin = [[IFPluginDatabase installedPlugins] pluginForMimeType:mimeType];
    } else {
        plugin = [[IFPluginDatabase installedPlugins] pluginForExtension:[pluginURL pathExtension]];
        mimeType = [plugin mimeTypeForURL:pluginURL];
    }
    
    if (plugin == nil) {
        return [[[IFNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) mimeType:mimeType arguments:arguments] autorelease];
    }
    return [[[IFPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) plugin:plugin url:[NSURL URLWithString:pluginURL] mime:mimeType arguments:arguments mode:NP_EMBED] autorelease];
}

- (NSArray *)pluginsInfo
{
    return [[IFPluginDatabase installedPlugins] plugins];
}

- (NSView *)viewForJavaAppletWithArguments:(NSDictionary *)arguments
{
    IFPlugin *plugin;
    NSMutableDictionary *argsCopy;
    
    plugin = [[IFPluginDatabase installedPlugins] pluginForFilename:@"Java.plugin"];
    if (plugin == nil) {
        return nil;
    }
    
    argsCopy = [NSMutableDictionary dictionaryWithDictionary:arguments];
    [argsCopy setObject:[argsCopy objectForKey:@"baseURL"] forKey:@"DOCBASE"];
    [argsCopy removeObjectForKey:@"baseURL"];

    if (plugin == nil) {
        return [[[IFNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) mimeType:@"application/x-java-applet" arguments:argsCopy] autorelease];
    }
    return [[[IFPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) plugin:plugin url:nil mime:@"application/x-java-applet" arguments:argsCopy mode:NP_EMBED] autorelease];
}

@end
