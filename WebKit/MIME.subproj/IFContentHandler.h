/*	
    IFContentHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFMIMEHandler.h>

@interface IFContentHandler : NSObject {
    IFMIMEHandlerType handlerType;
    NSString *MIMEType, *URLString;
}

- initWithURL:(NSURL *)URL MIMEType:(NSString *)theMIMEType MIMEHandlerType:(IFMIMEHandlerType)theMIMEHandlerType;
- (NSString *) HTMLDocument;
- (NSString *) textHTMLDocumentBottom;

@end
