//
//  WKPluginsDatabase.h
//  
//
//  Created by Chris Blumenberg on Tue Dec 11 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WKPlugin.h"

@interface WKPluginDatabase : NSObject {

    NSArray *plugins;
}

+ (WKPluginDatabase *)installedPlugins;
- (WKPlugin *)getPluginForMimeType:(NSString *)mimeType;

@end

NSMutableDictionary *getMimeTypesForResourceFile(SInt16 resRef);
NSArray *findPlugins(void);