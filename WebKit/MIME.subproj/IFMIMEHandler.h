//
//  IFMIMEHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import <Foundation/Foundation.h>

/*
IFMIMEHandler is a simple data type object holding the MIME type, the type of handler and the handler's name (ie plugin's name or application's name).
*/


typedef enum {
    IFMIMEHANDLERTYPE_HTML = 1,		//WebKit handled html
    IFMIMEHANDLERTYPE_IMAGE = 2,	//WebKit handled image
    IFMIMEHANDLERTYPE_TEXT = 3,		//WebKit handled text
    IFMIMEHANDLERTYPE_PLUG_IN = 4,	//Plug-in handled file
    IFMIMEHANDLERTYPE_APPLICATION = 5,	//Application handled file
} IFMIMEHandlerType;


@interface IFMIMEHandler : NSObject {
    NSString *MIMEType, *MIMESupertype, *MIMESubtype, *handlerName;
    IFMIMEHandlerType handlerType;
}

/*
initWithMIMEType gets called by [IFMIMEDatabase sharedMIMEDatabase] for at least every mime type that WebKit handles. We, at some point, might want to store IFMIMEHandler's for types that other application handle. I hope not though.
*/

- initWithMIMEType:(NSString *)MIME handlerType:(IFMIMEHandlerType)hType handlerName:(NSString *)handler;

// Accessor methods
- (NSString *)MIMEType;
- (NSString *)MIMESupertype;
- (NSString *)MIMESubtype;
- (NSString *)handlerName;
- (IFMIMEHandlerType)handlerType;

@end
