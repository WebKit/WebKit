/*	
    WebPDFRepresentation.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guaranateed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3

#import <Foundation/Foundation.h>

@protocol WebDocumentRepresentation;

@interface WebPDFRepresentation : NSObject <WebDocumentRepresentation>
{
}
@end

#endif  // MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
