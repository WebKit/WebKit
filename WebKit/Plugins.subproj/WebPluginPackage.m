//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
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

        description = [MIMEDictionary objectForKey:WebPluginDescriptionKey];
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
    
    bundle = [[NSBundle alloc] initWithPath:pluginPath];

    if(!bundle){
        return nil;
    }

    UInt32 type;
    CFBundleRef theBundle = CFBundleCreate(NULL, (CFURLRef)[NSURL fileURLWithPath:pluginPath]);        
    CFBundleGetPackageInfo(theBundle, &type, NULL);
    CFRelease(theBundle);

    [self setPath:pluginPath];
    
    if(type != FOUR_CHAR_CODE('WBPL') || ![self getMIMEInformation]){
        [bundle release];
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
