//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//


#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginView.h>
#import <WebKit/WebViewFactory.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebNSURLExtras.h>

@implementation WebViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    WEBKIT_ASSERT([[self sharedFactory] isMemberOfClass:self]);
}

- (NSView *)viewForPluginWithURL:(NSURL *)pluginURL serviceType:(NSString *)serviceType arguments:(NSArray *)args baseURL:(NSURL *)baseURL
{
    NSMutableDictionary *arguments;
    NSString *mimeType, *extension;
    NSRange r1, r2, r3;
    WebPlugin *plugin;
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
        
    if ([serviceType length]) {
        mimeType = serviceType;
        plugin = [[WebPluginDatabase installedPlugins] pluginForMimeType:mimeType];
    } else {
        extension = [[pluginURL path] pathExtension];
        plugin = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        mimeType = [plugin mimeTypeForExtension:extension];
    }
    
    if (plugin == nil) {
        return [[[WebNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) mimeType:mimeType arguments:arguments] autorelease];
    }
    return [[[WebPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0) plugin:plugin URL:pluginURL baseURL:baseURL mime:mimeType arguments:arguments] autorelease];
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame baseURL:(NSURL *)baseURL parameters:(NSDictionary *)parameters
{
    WebPlugin *plugin;
    
    plugin = [[WebPluginDatabase installedPlugins] pluginForMimeType:@"application/x-java-applet"];
    if (plugin == nil) {
        return nil;
    }
    
    return [[[WebPluginView alloc] initWithFrame:frame plugin:plugin URL:nil baseURL:baseURL mime:@"application/x-java-applet" arguments:parameters] autorelease];
}

@end
