/*	
    WebTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@protocol WebDocumentRepresentation;

@interface WebTextRepresentation : NSObject <WebDocumentRepresentation>
{
    NSString *RTFSource;
    BOOL hasRTFSource;
}
@end
