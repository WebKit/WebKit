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
IFMIMEDatabase creates a dictionary of MIME type keys and IFMIMEHandlers objects. IFMIMEDatabase should only create entries in the dictionary for mime types that WebKit handles internally including plug-in types.

At some point, IFMIMEDatabase might interact with the defaults to provide for customization.

Types that WebKit can/will handle internally (thanks to AppKit):

Text types:
text/html
text/plain
text/richtext
application/rtf

Image types (taken from [NSImage imageFileTypes]): 
image/pict
application/postscript (eps)
image/tiff
image/x-quicktime
image/x-targa
image/x-sgi
image/x-rgb
image/x-macpaint
image/png
image/gif
image/jpg
image/x-bmp
image/tiff
image/x-tiff

Image mime types not found, but handleable:
psd (photoshop)
fpx
cur
ico (window's icon)
fax

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
