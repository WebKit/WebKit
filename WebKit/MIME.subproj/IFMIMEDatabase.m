//
//  IFMIMEDatabase.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import "IFMIMEDatabase.h"

#import "IFMIMEHandler.h"
#import <WCPluginDatabase.h>
#import <WCPlugin.h>

NSMutableDictionary *setMimeHandlers(void);


static IFMIMEDatabase *sharedMIMEDatabase = nil;

@implementation IFMIMEDatabase

+ (IFMIMEDatabase *)sharedMIMEDatabase
{
    if(!sharedMIMEDatabase){
        sharedMIMEDatabase = [IFMIMEDatabase alloc];
        sharedMIMEDatabase->mimeHandlers = setMimeHandlers();
    }
    return sharedMIMEDatabase;
}


- (IFMIMEHandler *)MIMEHandlerForMIMEType:(NSString *)mimeType
{
    IFMIMEHandler *tempHandler;
    WCPluginDatabase *pluginDatabase;
    WCPlugin *plugin;
    
    if(!mimeType)
        return [[[IFMIMEHandler alloc] initWithMIMEType:mimeType handlerType:IFMIMEHANDLERTYPE_NIL handlerName:nil] autorelease];
    
    tempHandler = [mimeHandlers objectForKey:mimeType];
    if(tempHandler){
        return tempHandler;
    }else{
        pluginDatabase = [WCPluginDatabase installedPlugins];
        plugin = [pluginDatabase getPluginForMimeType:mimeType];
        if(plugin){
            return [[[IFMIMEHandler alloc] initWithMIMEType:mimeType handlerType:IFMIMEHANDLERTYPE_PLUGIN handlerName:[plugin name]] autorelease];
        }
        else{
            return [[[IFMIMEHandler alloc] initWithMIMEType:mimeType handlerType:IFMIMEHANDLERTYPE_APPLICATION handlerName:nil] autorelease];
        }
    }
}

@end

NSMutableDictionary *setMimeHandlers(void)
{
    NSArray *imageTypes;
    NSMutableDictionary *handledTypes;
    IFMIMEHandler *tempHandler;
    NSString *tempMime = nil;
    uint i;
    
    handledTypes = [NSMutableDictionary dictionaryWithCapacity:20];
    imageTypes = [NSArray arrayWithObjects:
        //@"image/pict",
        //@"application/postscript",
        //@"image/x-quicktime",
        //@"image/x-targa",
        //@"image/x-sgi",
        //@"image/x-rgb",
        //@"image/x-macpaint",
        //@"image/x-bmp",
        //@"image/tiff",
        //@"image/x-tiff",
        @"image/png",
        @"image/gif",
        @"image/jpg",
        @"image/jpeg", nil];

    for(i=0; i<[imageTypes count]; i++){
        tempMime = [imageTypes objectAtIndex:i];
        tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:tempMime handlerType:IFMIMEHANDLERTYPE_IMAGE handlerName:@"WebKit"];
        [handledTypes setObject:tempHandler forKey:tempMime];
    }
    tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:@"text/plain" handlerType:IFMIMEHANDLERTYPE_TEXT handlerName:@"WebKit"];
    [handledTypes setObject:tempHandler forKey:@"text/plain"];
    
    tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:@"text/html" handlerType:IFMIMEHANDLERTYPE_HTML handlerName:@"WebKit"];
    [handledTypes setObject:tempHandler forKey:@"text/html"];
    
    [handledTypes retain];
    
    return handledTypes;
}