/*	
    WebPDFView.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/
#ifndef OMIT_TIGER_FEATURES

#import <Quartz/Quartz.h>

@class WebDataSource;

@interface WebPDFView : PDFView <WebDocumentView, WebDocumentSearching>
{
    WebDataSource *dataSource;
    NSString *path;
    BOOL written;
}
@end

#endif  // OMIT_TIGER_FEATURES
