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


+ (WCPluginDatabase *)installedPlugins 
{
    if(!__WCPluginDatabase){
        __WCPluginDatabase  = [WCPluginDatabase alloc];
        __WCPluginDatabase->plugins = findPlugins();
    }
    return __WCPluginDatabase;
}

// The first plugin with the specified mime type is returned. We may want to tie this to the defaults so that this is configurable.
- (WCPlugin *)getPluginForMimeType:(NSString *)mimeType
{
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

- (WCPlugin *)getPluginForExtension:(NSString *)extension
{
    uint i, n;
    WCPlugin *plugin;
    NSArray *mimeArray;
    NSRange hasExtension;

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

- (WCPlugin *)getPluginForFilename:(NSString *)filename
{
    uint i;
    WCPlugin *plugin;
    
    for(i=0; i<[plugins count]; i++){
        plugin = [plugins objectAtIndex:i];
        if([[plugin filename] isEqualToString:filename]){
            return plugin;
        }
    }
    return nil;
}

- (NSArray *) plugins
{
    return plugins;
}

@end

NSArray *pluginLocations(void)
{
    NSMutableArray *locations;
    NSBundle *applicationBundle;
    
    // Plug-ins are found in order of precedence.
    // If there are duplicates, the first found plug-in is used.
    // For example, if there is a QuickTime.plugin in the users's home directory
    // that is used instead of the /Library/Internet Plug-ins version.
    // The purpose is to allow non-admin user's to update their plug-ins.
    
    locations = [NSMutableArray arrayWithCapacity:3];
    [locations addObject:[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Internet Plug-Ins"]];
    [locations addObject:@"/Library/Internet Plug-Ins"];

    applicationBundle = [NSBundle mainBundle];
    [locations addObject:[applicationBundle builtInPlugInsPath]];
    
    return locations;
}

NSArray *findPlugins(void)
{
    NSFileManager *fileManager;
    NSArray *pluginDirectories, *files;
    NSString *file;
    NSMutableArray *pluginPaths, *pluginArray, *filenames;
    WCPlugin *plugin;
    uint i, n;
    
    pluginDirectories = pluginLocations();    
    fileManager = [NSFileManager defaultManager];
    pluginPaths = [NSMutableArray arrayWithCapacity:10];
    filenames = [NSMutableArray arrayWithCapacity:10];
    
    for(i=0; i<[pluginDirectories count]; i++){
        files = [fileManager directoryContentsAtPath:[pluginDirectories objectAtIndex:i]];
        for(n=0; n<[files count]; n++){
            file = [files objectAtIndex:n];
            if(![filenames containsObject:file]){ // avoid duplicates
                [filenames addObject:file];
                [pluginPaths addObject:[[pluginDirectories objectAtIndex:i] stringByAppendingPathComponent:file]];
            }
        }
    }
    
    pluginArray = [NSMutableArray arrayWithCapacity:[pluginPaths count]];
    
    for(i=0; i<[pluginPaths count]; i++){
        plugin = [WCPlugin alloc];
        if([plugin initializeWithPath:[pluginPaths objectAtIndex:i]]){
            [plugin retain];
            [pluginArray addObject:plugin];
            KWQDEBUG("Found plugin: %s\n", [[plugin name] lossyCString]);
            KWQDEBUG("%s", [[plugin description] lossyCString]);
        }
    }
    return [pluginArray retain];
}


