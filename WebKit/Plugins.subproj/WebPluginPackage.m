//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginPackage.h>

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>

#import <Foundation/NSPrivateDecls.h>

NSString *WebPlugInBaseURLKey =     @"WebPlugInBaseURLKey";
NSString *WebPlugInAttributesKey =  @"WebPlugInAttributesKey";
NSString *WebPlugInContainerKey =   @"WebPlugInContainerKey";
NSString *WebPlugInModeKey =        @"WebPlugInModeKey";

/*!
	@constant WebPlugInContainingElementKey The DOMElement that was used to specify
	the plug-in.  May be nil.
*/
extern NSString *WebPlugInContainingElementKey;

@implementation WebPluginPackage

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];

    if (bundle == nil) {
        [self release];
        return nil;
    }
    
    if (![[pluginPath pathExtension] _webkit_isCaseInsensitiveEqualToString:@"webplugin"]) {
        UInt32 type = 0;
        CFBundleGetPackageInfo([bundle _cfBundle], &type, NULL);
        if (type != FOUR_CHAR_CODE('WBPL')) {
            [self release];
            return nil;
        }
    }

    if (![self getPluginInfoFromPLists]) {
        [self release];
        return nil;
    }

    return self;
}

- (Class)viewFactory
{
    return [bundle principalClass];
}

- (BOOL)load
{
    if (isLoaded) {
        return YES;
    }
    
#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    
    [bundle principalClass];
    isLoaded = [bundle isLoaded];
    if (!isLoaded) {
        return NO;
    }

#if !LOG_DISABLED
    CFAbsoluteTime duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Plugins, "principalClass took %f seconds for: %@", duration, [self name]);
#endif
    return [super load];
}

- (void)unload
{
}

- (BOOL)isLoaded
{
    return [bundle isLoaded];
}

@end

@implementation NSObject (WebScripting)

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return YES;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return YES;
}

@end

