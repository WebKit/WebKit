/*	
        WebHTMLRepresentationPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLRepresentation.h>

@class WebBridge;

@interface WebHTMLRepresentation (WebPrivate)
- (WebBridge *)_bridge;
- (void)printDOMTree;
@end
