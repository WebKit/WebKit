/*	
    WebPDFView.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guaranateed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3

#import <Quartz/Quartz.h>

@class WebDataSource;

@interface WebPDFView : PDFView <WebDocumentView, WebDocumentSearching>
{
    WebDataSource *dataSource;
    NSString *path;
    BOOL written;
}
@end

#endif  // MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
