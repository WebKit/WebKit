//
//  IFMIMEDatabase.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <IFMIMEHandler.h>

@interface IFMIMEDatabase : NSObject {
    NSMutableDictionary *mimeHandlers;
}

/*
sharedMIMEDatabase creates a dictionary of MIME type keys and IFMIMEHandlers objects. sharedMIMEDatabase should only create entries in the dictionary for mime types that WebKit handles internally including plug-in types.

At some point, sharedMIMEDatabase might interact with the defaults to provide for customization.
*/

+ (IFMIMEDatabase *)sharedMIMEDatabase;


/* 
MIMEHandlerForMIMEType does a simple dictionary lookup. If none is found IFMIMEHandler will return a IFMIMEHandler with an application handler type (look at IFMIMEHandler.h) which means that the file should be downloaded.
*/

- (IFMIMEHandler *)MIMEHandlerForMIMEType:(NSString *)mimeType;


/* 
If a mime type is not available, MIMEHandlerForURL determines a IFMIMEHandler based on the URL's extension. This may require interaction with Launch Services.
*/

- (IFMIMEHandler *)MIMEHandlerForURL:(NSURL *)url;

@end
