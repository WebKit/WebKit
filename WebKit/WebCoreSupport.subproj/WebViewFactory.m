//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebBasePluginPackage.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebViewFactory.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSURLExtras.h>

@implementation WebViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (NSView *)viewForPluginWithURL:(NSURL *)pluginURL serviceType:(NSString *)serviceType arguments:(NSArray *)args baseURL:(NSURL *)baseURL
{
    NSMutableDictionary *arguments;
    NSString *mimeType, *extension;
    NSRange r1, r2, r3;
    WebBasePluginPackage *pluginPackage;
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
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:mimeType];
    } else {
        extension = [[pluginURL path] pathExtension];
        pluginPackage = [[WebPluginDatabase installedPlugins] pluginForExtension:extension];
        mimeType = [[pluginPackage extensionToMIMEDictionary] objectForKey:extension];
    }
    
    if (pluginPackage) {
        if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
            return nil;
        }else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
            return [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSMakeRect(0,0,0,0)
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:pluginURL
                                                                 baseURL:baseURL
                                                                    mime:mimeType
                                                               arguments:arguments] autorelease];
        }else{
            [NSException raise:NSInternalInconsistencyException
                        format:@"Plugin package class not recognized"];
            return nil;
        }
    }else{
        return [[[WebNullPluginView alloc] initWithFrame:NSMakeRect(0,0,0,0)
                                                mimeType:mimeType
                                               arguments:arguments] autorelease];
    }
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame baseURL:(NSURL *)baseURL parameters:(NSDictionary *)parameters
{
    WebBasePluginPackage *pluginPackage;
    
    pluginPackage = [[WebPluginDatabase installedPlugins] pluginForMIMEType:@"application/x-java-applet"];

    if (!pluginPackage) {
        return nil;
    }

    if([pluginPackage isKindOfClass:[WebPluginPackage class]]){
        return nil;
    }else if([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]){
        return [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:frame
                                                              plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                 URL:nil
                                                             baseURL:baseURL
                                                                mime:@"application/x-java-applet"
                                                           arguments:parameters] autorelease];
    }else{
        [NSException raise:NSInternalInconsistencyException
                    format:@"Plugin package class not recognized"];
        return nil;
    }
}

- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result
{
    WebJavaScriptTextInputPanel *panel = [[WebJavaScriptTextInputPanel alloc] initWithPrompt:prompt text:defaultText];
    [panel showWindow:nil];
    BOOL OK = [NSApp runModalForWindow:[panel window]];
    if (OK) {
        *result = [panel text];
    }
    [panel release];
    return OK;
}

@end
