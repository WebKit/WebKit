//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebPluginPackage.h>


@implementation WebPluginPackage

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];
    
    bundle = [[NSBundle alloc] initWithPath:pluginPath];

    if(!bundle){
        return nil;
    }
    
    CFBundleRef theBundle = CFBundleCreate(NULL, (CFURLRef)[NSURL fileURLWithPath:pluginPath]);
    UInt32 type;
        
    CFBundleGetPackageInfo(theBundle, &type, NULL);

    CFRelease(theBundle);

    if(type != FOUR_CHAR_CODE('WBPL')){
        [bundle release];
        return nil;
    }

    return self;
}

@end
