//
//  IFMIMEDatabase.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import "IFMIMEDatabase.h"

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
    tempHandler = [mimeHandlers objectForKey:mimeType];
    if(tempHandler)
        return tempHandler;
    else
        return [[IFMIMEHandler alloc] initWithMIMEType:mimeType handlerType:IFMIMEHANDLERTYPE_APPLICATION handlerName:@""];
}


- (IFMIMEHandler *)MIMEHandlerForURL:(NSURL *)url
{
    return nil;
}

@end

NSMutableDictionary *setMimeHandlers(void)
{
    NSArray *textTypes, *imageTypes;
    NSMutableDictionary *handledTypes;
    IFMIMEHandler *tempHandler;
    NSString *tempMime = nil;
    uint i;
    
    handledTypes = [NSMutableDictionary dictionaryWithCapacity:20];
    textTypes = [NSArray arrayWithObjects:@"text/plain", @"text/richtext", @"application/rtf", nil];
    imageTypes = [NSArray arrayWithObjects:
        @"image/pict",
        @"application/postscript",
        @"image/tiff",
        @"image/x-quicktime",
        @"image/x-targa",
        @"image/x-sgi",
        @"image/x-rgb",
        @"image/x-macpaint",
        @"image/png",
        @"image/gif",
        @"image/jpg",
        @"image/x-bmp",
        @"image/tiff",
        @"image/x-tiff", nil];

    for(i=0; i<[textTypes count]; i++){
        tempMime = [textTypes objectAtIndex:i];
        tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:tempMime handlerType:IFMIMEHANDLERTYPE_TEXT handlerName:@"WebKit"];
        [handledTypes setObject:tempHandler forKey:tempMime];
    }
    for(i=0; i<[imageTypes count]; i++){
        tempMime = [imageTypes objectAtIndex:i];
        tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:tempMime handlerType:IFMIMEHANDLERTYPE_IMAGE handlerName:@"WebKit"];
        [handledTypes setObject:tempHandler forKey:tempMime];
    }
    tempHandler = [[IFMIMEHandler alloc] initWithMIMEType:@"text/html" handlerType:IFMIMEHANDLERTYPE_HTML handlerName:@"WebKit"];
    [handledTypes setObject:tempHandler forKey:@"text/html"];
    
    [handledTypes retain];
    
    return handledTypes;
}