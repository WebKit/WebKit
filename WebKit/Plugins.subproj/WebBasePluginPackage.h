//
//  WebBasePluginPackage.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebCore/WebCoreViewFactory.h>

#define WebPluginExtensionsKey		@"WebPluginExtensions"
#define WebPluginDescriptionKey 	@"WebPluginDescription"
#define WebPluginLocalizationNameKey	@"WebPluginLocalizationName"
#define WebPluginMIMETypesFilenameKey	@"WebPluginMIMETypesFilename"
#define WebPluginMIMETypesKey 		@"WebPluginMIMETypes"
#define WebPluginNameKey 		@"WebPluginName"
#define WebPluginTypeDescriptionKey 	@"WebPluginTypeDescription"
#define WebPluginTypeEnabledKey 	@"WebPluginTypeEnabled"

@interface WebBasePluginPackage : NSObject <WebCorePluginInfo>
{
    NSString *name;
    NSString *path;
    NSString *pluginDescription;

    NSBundle *bundle;

    NSDictionary *MIMEToDescription;
    NSDictionary *MIMEToExtensions;
    NSMutableDictionary *extensionToMIME;
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath;
- (id)initWithPath:(NSString *)pluginPath;

- (BOOL)getPluginInfoFromBundleAndMIMEDictionary:(NSDictionary *)MIMETypes;

- (BOOL)load;
- (void)unload;
- (BOOL)isLoaded;

- (NSString *)name;
- (NSString *)path;
- (NSString *)filename;
- (NSString *)pluginDescription;
- (NSBundle *)bundle;

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
