 //
//  WKPluginsDatabase.m
//  
//
//  Created by Chris Blumenberg on Tue Dec 11 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKPluginDatabase.h"
#include "kwqdebug.h"

@implementation WKPluginDatabase
static WKPluginDatabase *__WKPluginDatabase = nil;


+ (WKPluginDatabase *)installedPlugins {

    if(!__WKPluginDatabase){
        __WKPluginDatabase  = [WKPluginDatabase alloc];
        __WKPluginDatabase->plugins = findPlugins();
    }
    return __WKPluginDatabase;
}

// The first plugin with the specified mime type is returned. We may want to tie this to the defaults so that this is configurable.
- (WKPlugin *)getPluginForMimeType:(NSString *)mimeType{
    uint i, n;
    WKPlugin *plugin;
    NSArray *mimeArray;
    
    for(i=0; i<[plugins count]; i++){      
        plugin = [plugins objectAtIndex:i];
        mimeArray = [plugin mimeTypes];
        for(n=0; n<[mimeArray count]; n++){
            if([[[mimeArray objectAtIndex:n] objectAtIndex:0] isEqualToString:mimeType]){
                return plugin;
            }
        }
    }
    return nil;
}

- (WKPlugin *)getPluginForURL:(NSString *)URL{
    uint i, n;
    WKPlugin *plugin;
    NSArray *mimeArray;
    NSRange hasExtension;
    NSString *extension;
    
    extension = [URL pathExtension];
    for(i=0; i<[plugins count]; i++){      
        plugin = [plugins objectAtIndex:i];
        mimeArray = [plugin mimeTypes];
        for(n=0; n<[mimeArray count]; n++){
            hasExtension = [[[mimeArray objectAtIndex:n] objectAtIndex:1] rangeOfString:extension];
            if(hasExtension.length){
                return plugin;
            }
        }
    }
    return nil;
}



- (NSArray *) plugins{
    return plugins;
}

@end

NSArray *findPlugins(void){
    NSFileManager *fileManager;
    NSArray *libraryPlugins, *homePlugins;
    NSString *homePluginDir, *libPluginDir;
    NSMutableArray *pluginPaths, *pluginArray;
    WKPlugin *plugin;
    uint i;
    
    fileManager = [NSFileManager defaultManager];
    homePluginDir = [[NSString stringWithString:NSHomeDirectory()] stringByAppendingPathComponent:@"Library/Internet Plug-Ins"];
    libPluginDir = [NSString stringWithString:@"/Library/Internet Plug-Ins"];
    
    homePlugins = [fileManager directoryContentsAtPath:homePluginDir];
    libraryPlugins = [fileManager directoryContentsAtPath:libPluginDir];

    pluginPaths = [NSMutableArray arrayWithCapacity:[homePlugins count]+[libraryPlugins count]];

    for(i=0; i<[homePlugins count]; i++){
        [pluginPaths addObject:[homePluginDir stringByAppendingPathComponent:[homePlugins objectAtIndex:i]]];
    }
    for(i=0; i<[libraryPlugins count]; i++){
        if(![homePlugins containsObject:[libraryPlugins objectAtIndex:i]]){ // avoid dups, give precedense to home dir
            [pluginPaths addObject:[libPluginDir stringByAppendingPathComponent:[libraryPlugins objectAtIndex:i]]];
        }
    }
    pluginArray = [NSMutableArray arrayWithCapacity:[pluginPaths count]];
    
    for(i=0; i<[pluginPaths count]; i++){
        plugin = [WKPlugin alloc];
        if([plugin initializeWithPath:[pluginPaths objectAtIndex:i]]){
            [plugin retain];
            [pluginArray addObject:plugin];
            KWQDebug("Found plugin: %s\n", [[plugin name] cString]);
        }
    }
    return [pluginArray retain];
}


