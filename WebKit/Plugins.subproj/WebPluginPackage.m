//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginPackage.h>

@implementation WebPluginPackage

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];

    if (!nsBundle) {
        [self release];
        return nil;
    }
    
    UInt32 type = 0;
    CFBundleRef cfBundle = CFBundleCreate(NULL, (CFURLRef)[NSURL fileURLWithPath:path]);        
    if (cfBundle) {
        CFBundleGetPackageInfo(cfBundle, &type, NULL);
        CFRelease(cfBundle);
    }
    
    if (type != FOUR_CHAR_CODE('WBPL')) {
        [self release];
        return nil;
    }

    if (![self getPluginInfoFromBundleAndMIMEDictionary:nil]) {
        [self release];
        return nil;
    }

    return self;
}

- (Class)viewFactory
{
    return [nsBundle principalClass];
}

- (BOOL)load
{
    [nsBundle principalClass];
    return YES;
}

- (void)unload
{
}

- (BOOL)isLoaded
{
    return [nsBundle isLoaded];
}

@end
