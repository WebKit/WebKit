//
//  WebBasePluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBasePluginPackage.h>

#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginPackage.h>


@implementation WebBasePluginPackage

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath
{
    WebBasePluginPackage *pluginPackage = [[WebPluginPackage alloc] initWithPath:pluginPath];

    if (!pluginPackage) {
        pluginPackage = [[WebNetscapePluginPackage alloc] initWithPath:pluginPath];
    }

    return [pluginPackage autorelease];
}

- (NSString *)pathByResolvingSymlinksAndAliasesInPath:(NSString *)thePath
{
    NSString *newPath = [thePath stringByResolvingSymlinksInPath];

    FSRef fref;
    OSErr err;

    err = FSPathMakeRef((const UInt8 *)[thePath fileSystemRepresentation], &fref, NULL);
    if (err != noErr) {
        return newPath;
    }

    Boolean targetIsFolder;
    Boolean wasAliased;
    err = FSResolveAliasFile (&fref, TRUE, &targetIsFolder, &wasAliased);
    if (err != noErr) {
        return newPath;
    }

    if (wasAliased) {
        NSURL *URL = (NSURL *)CFURLCreateFromFSRef(kCFAllocatorDefault, &fref);
        newPath = [URL path];
        [URL release];
    }

    return newPath;
}

- initWithPath:(NSString *)pluginPath
{
    [super init];
    extensionToMIME = [[NSMutableDictionary dictionary] retain];
    path = [[self pathByResolvingSymlinksAndAliasesInPath:pluginPath] retain];
    bundle = [[NSBundle alloc] initWithPath:path];
    return self;
}

- (BOOL)getPluginInfoFromBundleAndMIMEDictionary:(NSDictionary *)MIMETypes
{
    if (!bundle) {
        return NO;
    }
    
    if (!MIMETypes) {
        MIMETypes = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesKey];
        if (!MIMETypes) {
            return NO;
        }
    }

    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];
    NSEnumerator *keyEnumerator = [MIMETypes keyEnumerator];
    NSDictionary *MIMEDictionary;
    NSString *MIME, *description;
    NSArray *extensions;

    while ((MIME = [keyEnumerator nextObject]) != nil) {
        MIMEDictionary = [MIMETypes objectForKey:MIME];

        // FIXME: Consider storing disabled MIME types.
        NSNumber *isEnabled = [MIMEDictionary objectForKey:WebPluginTypeEnabledKey];
        if (isEnabled && [isEnabled boolValue] == NO) {
            continue;
        }

        extensions = [MIMEDictionary objectForKey:WebPluginExtensionsKey];
        if (!extensions) {
            extensions = [NSArray arrayWithObject:@""];
        }

        [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];

        description = [MIMEDictionary objectForKey:WebPluginTypeDescriptionKey];
        if (!description) {
            description = @"";
        }

        [MIMEToDescriptionDictionary setObject:description forKey:MIME];
    }

    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];
    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];

    NSString *filename = [self filename];

    NSString *theName = [bundle objectForInfoDictionaryKey:WebPluginNameKey];
    if (!theName) {
        theName = filename;
    }
    [self setName:theName];

    description = [bundle objectForInfoDictionaryKey:WebPluginDescriptionKey];
    if (!description) {
        description = filename;
    }
    [self setPluginDescription:description];

    return YES;
}

- (BOOL)isLoaded
{
    return NO;
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

    [bundle release];
    
    [super dealloc];
}

- (NSString *)name
{
    return name;
}

- (NSString *)path
{
    return path;
}

- (NSString *)filename
{
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
            if (![extension isEqualToString:@""]) {
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
