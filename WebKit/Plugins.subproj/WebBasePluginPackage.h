//
//  WebBasePluginPackage.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebCore/WebCoreViewFactory.h>

@interface WebBasePluginPackage : NSObject <WebCorePluginInfo>
{
    NSString *name;
    NSString *path;
    NSString *pluginDescription;

    NSDictionary *MIMEToDescription;
    NSDictionary *MIMEToExtensions;
    NSMutableDictionary *extensionToMIME;
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath;

- initWithPath:(NSString *)pluginPath;

- (BOOL)load;
- (void)unload;

- (NSString *)name;
- (NSString *)path;
- (NSString *)filename;
- (NSString *)pluginDescription;

- (NSEnumerator *)extensionEnumerator;
- (NSEnumerator *)MIMETypeEnumerator;
- (NSString *)descriptionForMIMEType:(NSString *)MIMEType;
- (NSString *)MIMETypeForExtension:(NSString *)extension;
- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType;

- (void)setName:(NSString *)theName;
- (void)setPath:(NSString *)thePath;
- (void)setPluginDescription:(NSString *)description;
- (void)setMIMEToDescriptionDictionary:(NSDictionary *)MIMEToDescriptionDictionary;
- (void)setMIMEToExtensionsDictionary:(NSDictionary *)MIMEToExtensionsDictionary;

@end
