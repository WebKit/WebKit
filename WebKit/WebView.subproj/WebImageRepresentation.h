/*	
    WebImageRepresentation.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class WebArchive;
@class WebDataSource;
@class WebImageRenderer;

@protocol WebDocumentRepresentation;

@interface WebImageRepresentation : NSObject <WebDocumentRepresentation>
{
    WebDataSource *dataSource;
    WebImageRenderer *image;
    BOOL doneLoading;
}
- (WebImageRenderer *)image;
- (NSString *)filename;
- (WebArchive *)archive;
- (BOOL)doneLoading;
- (NSData *)data;
- (NSURL *)URL;
@end
