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
    extensionToMIME = [[NSMutableDictionary dictionary] retain];
    return self;
}

- (BOOL)load
{
    return NO;
}

- (void)unload
{
}

- (void)dealloc
{
    [name release];
    [path release];
    [pluginDescription release];

    [MIMEToDescription release];
    [MIMEToExtensions release];
    [extensionToMIME release];
    [super dealloc];
}


- (NSString *)name{
    return name;
}

- (NSString *)path{
    return path;
}

- (NSString *)filename{
    return [path lastPathComponent];
}

- (NSString *)pluginDescription
{
    return pluginDescription;
}

- (NSEnumerator *)extensionEnumerator
{
    return [extensionToMIME keyEnumerator];
}

- (NSEnumerator *)MIMETypeEnumerator
{
    return [MIMEToExtensions keyEnumerator];
}

- (NSString *)descriptionForMIMEType:(NSString *)MIMEType
{
    return [MIMEToDescription objectForKey:MIMEType];
}

- (NSString *)MIMETypeForExtension:(NSString *)extension
{
    return [extensionToMIME objectForKey:extension];
}

- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType
{
    return [MIMEToExtensions objectForKey:MIMEType];
}

- (void)setName:(NSString *)theName
{
    [name release];
    name = [theName retain];
}

- (void)setPath:(NSString *)thePath
{
    [path release];
    path = [thePath retain];
}

- (void)setPluginDescription:(NSString *)description
{
    [pluginDescription release];
    pluginDescription = [description retain];
}

- (void)setMIMEToDescriptionDictionary:(NSDictionary *)MIMEToDescriptionDictionary
{
    [MIMEToDescription release];
    MIMEToDescription = [MIMEToDescriptionDictionary retain];
}

- (void)setMIMEToExtensionsDictionary:(NSDictionary *)MIMEToExtensionsDictionary
{
    [MIMEToExtensions release];
    MIMEToExtensions = [MIMEToExtensionsDictionary retain];

    // Reverse the mapping
    [extensionToMIME removeAllObjects];

    NSEnumerator *MIMEEnumerator = [MIMEToExtensions keyEnumerator], *extensionEnumerator;
    NSString *MIME, *extension;
    NSArray *extensions;
    
    while ((MIME = [MIMEEnumerator nextObject]) != nil) {
        extensions = [MIMEToExtensions objectForKey:MIME];
        extensionEnumerator = [extensions objectEnumerator];

        while ((extension = [extensionEnumerator nextObject]) != nil) {
            if(![extension isEqualToString:@""]){
                [extensionToMIME setObject:MIME forKey:extension];
            }
        }
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"name: %@\npath: %@\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, [MIMEToExtensions description], [MIMEToDescription description], pluginDescription];
}

@end
