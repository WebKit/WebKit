/*	
    IFContentHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFMIMEHandler.h>

@interface IFContentHandler : NSObject {
    IFMIMEHandlerType handlerType;
    NSString *MIMEType, *URLString;
}

- initWithMIMEHandler:(IFMIMEHandler *)mimeHandler URL:(NSURL *)URL;
- (NSString *) HTMLDocument;
- (NSString *) textHTMLDocumentBottom;

@end
