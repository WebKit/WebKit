//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginPackage.h>

#define WebPluginMIMETypesKey 		@"WebPluginMIMETypes"
#define WebPluginNameKey 		@"WebPluginName"
#define WebPluginDescriptionKey 	@"WebPluginDescription"
#define WebPluginExtensionsKey 		@"WebPluginExtensions"
#define WebPluginTypeDescriptionKey 	@"WebPluginTypeDescription"

@implementation WebPluginPackage

- (BOOL)getMIMEInformation
{
    NSDictionary *MIMETypes = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesKey];
    if(!MIMETypes){
        return NO;
    }

    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];
    NSEnumerator *keyEnumerator = [MIMETypes keyEnumerator];
    NSDictionary *MIMEDictionary;
    NSString *MIME, *description;
    NSArray *extensions;
    
    while ((MIME = [keyEnumerator nextObject]) != nil) {
        MIMEDictionary = [MIMETypes objectForKey:MIME];

        extensions = [MIMEDictionary objectForKey:WebPluginExtensionsKey];
        if(!extensions){
            extensions = [NSArray arrayWithObject:@""];
        }

        [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];

        description = [MIMEDictionary objectForKey:WebPluginTypeDescriptionKey];
        if(!description){
            description = @"";
        }

        [MIMEToDescriptionDictionary setObject:description forKey:MIME];
    }

    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];
    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];

    NSString *filename = [self filename];
    
    NSString *theName = [bundle objectForInfoDictionaryKey:WebPluginNameKey];
    if(!theName){
        theName = filename;
    }
    [self setName:theName];

    description = [bundle objectForInfoDictionaryKey:WebPluginDescriptionKey];
    if(!description){
        description = filename;
    }
    [self setPluginDescription:description];

    return YES;
}

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];
    
    UInt32 type = 0;
    CFBundleRef coreFoundationBundle = CFBundleCreate(NULL, (CFURLRef)[NSURL fileURLWithPath:pluginPath]);        
    if (coreFoundationBundle) {
        CFBundleGetPackageInfo(coreFoundationBundle, &type, NULL);
        CFRelease(coreFoundationBundle);
    }
    
    if (type != FOUR_CHAR_CODE('WBPL')) {
        [self release];
        return nil;
    }

    bundle = [[NSBundle alloc] initWithPath:pluginPath];
    if (!bundle) {
        [self release];
        return nil;
    }

    [self setPath:pluginPath];

    if (![self getMIMEInformation]) {
        [self release];
        return nil;
    }

    return self;
}

- (void)dealloc
{
    [bundle release];
    [super dealloc];
}

- (Class)viewFactory
{
    return [bundle principalClass];
}

- (BOOL)load
{
    return YES;
}

- (void)unload
{
}

@end
