//
//  IFMIMEHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import "IFMIMEHandler.h"


@implementation IFMIMEHandler


- initWithMIMEType:(NSString *)MIME handlerType:(IFMIMEHandlerType)hType handlerName:(NSString *)handler
{
    NSRange r;
    
    MIMEType = MIME;
    handlerType = hType;
    handlerName = handler;
    
    r = [MIMEType rangeOfString:@"/"];
    MIMESupertype = [MIMEType substringToIndex:r.location];
    MIMESubtype =   [MIMEType substringFromIndex:r.location];
    
    return self;
}


// Accessor methods
- (NSString *)MIMEType
{
    return MIMEType;
}


- (NSString *)MIMESupertype
{
    return MIMESupertype;
}


- (NSString *)MIMESubtype
{
    return MIMESubtype;
}


- (NSString *)handlerName
{
    return handlerName;
}


- (IFMIMEHandlerType)handlerType
{
    return handlerType;
}

- (NSString *) description
{
    NSString *handlerTypeString = nil;
    if(handlerType == IFMIMEHANDLERTYPE_HTML)
        handlerTypeString = @"IFMIMEHANDLERTYPE_HTML";
    else if(handlerType == IFMIMEHANDLERTYPE_IMAGE)
        handlerTypeString = @"IFMIMEHANDLERTYPE_IMAGE";
    else if(handlerType == IFMIMEHANDLERTYPE_TEXT)
        handlerTypeString = @"IFMIMEHANDLERTYPE_TEXT";
    else if(handlerType == IFMIMEHANDLERTYPE_PLUG_IN)
        handlerTypeString = @"IFMIMEHANDLERTYPE_PLUG_IN";
    else if(handlerType == IFMIMEHANDLERTYPE_APPLICATION)
        handlerTypeString = @"IFMIMEHANDLERTYPE_APPLICATION";
    return [NSString stringWithFormat:@"MIME TYPE: %@, HANDLER TYPE: %@, HANDLER NAME: %@", MIMEType, handlerTypeString, handlerName];
}

@end
