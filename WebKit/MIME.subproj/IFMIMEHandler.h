//
//  IFMIMEHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import <Foundation/Foundation.h>

// FIXME: This code should be replaced by the MIME type DB 2927855

typedef enum {
    IFMIMEHANDLERTYPE_NIL = 0,		
    IFMIMEHANDLERTYPE_HTML = 1,		//WebKit handled html
    IFMIMEHANDLERTYPE_IMAGE = 2,	//WebKit handled image
    IFMIMEHANDLERTYPE_TEXT = 3,		//WebKit handled text
    IFMIMEHANDLERTYPE_PLUGIN = 4,	//Plug-in handled file
    IFMIMEHANDLERTYPE_APPLICATION = 5,	//Application handled file
} IFMIMEHandlerType;


@interface IFMIMEHandler: NSObject

+ (IFMIMEHandlerType) MIMEHandlerTypeForMIMEType:(NSString *)MIMEType;
+ (BOOL) canShowMIMEType:(NSString *)MIMEType;

@end
