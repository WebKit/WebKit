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
    NSString *filename;
    NSData *data;
    NSURL *URL;
}
- (WebImageRenderer *)image;
- (NSString *)filename;
- (BOOL)doneLoading;
- (NSData *)data;
- (NSURL *)URL;
@end
