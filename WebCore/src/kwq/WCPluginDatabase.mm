/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WCPluginDatabase.h"
#include "kwqdebug.h"

@implementation WCPluginDatabase
static WCPluginDatabase *__WCPluginDatabase = nil;


+ (WCPluginDatabase *)installedPlugins {

    if(!__WCPluginDatabase){
        __WCPluginDatabase  = [WCPluginDatabase alloc];
        __WCPluginDatabase->plugins = findPlugins();
    }
    return __WCPluginDatabase;
}

// The first plugin with the specified mime type is returned. We may want to tie this to the defaults so that this is configurable.
- (WCPlugin *)getPluginForMimeType:(NSString *)mimeType{
    uint i, n;
    WCPlugin *plugin;
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

- (WCPlugin *)getPluginForURL:(NSString *)URL{
    uint i, n;
    WCPlugin *plugin;
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
    WCPlugin *plugin;
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
        plugin = [WCPlugin alloc];
        if([plugin initializeWithPath:[pluginPaths objectAtIndex:i]]){
            [plugin retain];
            [pluginArray addObject:plugin];
            KWQDebug("Found plugin: %s\n", [[plugin name] cString]);
        }
    }
    return [pluginArray retain];
}


