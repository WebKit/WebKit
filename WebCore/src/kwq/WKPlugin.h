//
//  WKPlugins.h
//  
//
//  Created by Chris Blumenberg on Tue Dec 11 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "npapi.h"

@interface WKPlugin : NSObject {

    NSDictionary *mimeTypes;
    NSString *name;
    NSString *executablePath;
    BOOL isLoaded;
    
}

- (BOOL)initializeWithPath:(NSString *)plugin;
- (void)load;
- (void)newInstance:(NPP)instance withType:(NSString *)mimeType withMode:(uint16)mode withArguments:(NSArray *)arguments withValues:(NSArray *)values;
- (void)destroyInstance:(NPP)instance;
- (void)unload;

- (NSDictionary *)mimeTypes;
- (NSString *)name;
- (NSString *)executablePath;
- (BOOL)isLoaded;
- (NSString *)description;


@end
    
NSMutableDictionary *getMimeTypesForResourceFile(SInt16 resRef);
NPError InitializePlugin(mainFuncPtr pluginMainFunc);