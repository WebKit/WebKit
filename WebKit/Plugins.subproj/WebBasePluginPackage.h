//
//  WebBasePluginPackage.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebCore/WebCoreViewFactory.h>

@interface WebBasePluginPackage : NSObject <WebCorePluginInfo>
{
    NSMutableDictionary *MIMEToExtensions;
    NSMutableDictionary *extensionToMIME;
    NSMutableDictionary *MIMEToDescription;

    NSString *name;
    NSString *path;
    NSString *filename;
    NSString *pluginDescription;
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath;

- initWithPath:(NSString *)pluginPath;

- (NSString *)name;
- (NSString *)path;
- (NSString *)filename;
- (NSString *)pluginDescription;
- (NSDictionary *)extensionToMIMEDictionary;
- (NSDictionary *)MIMEToExtensionsDictionary;
- (NSDictionary *)MIMEToDescriptionDictionary;

@end
