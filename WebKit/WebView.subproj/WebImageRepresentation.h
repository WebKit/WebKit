/*	
    WebImageRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebImageRenderer;
@protocol WebDocumentRepresentation;

@interface WebImageRepresentation : NSObject <WebDocumentRepresentation>
{
    WebImageRenderer *image;
    BOOL doneLoading;
    NSURL *URL;
}
- (WebImageRenderer *)image;
- (BOOL)doneLoading;
- (NSURL *)URL;
@end
