/*	
    IFHTMLRepresentationPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFHTMLRepresentation.h>

@class IFWebCoreBridge;

@interface IFHTMLRepresentation (IFPrivate)
- (IFWebCoreBridge *)_bridge;
@end
