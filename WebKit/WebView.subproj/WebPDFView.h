/*	
    WebPDFView.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/
#ifndef OMIT_TIGER_FEATURES

@class PDFView;
@class WebDataSource;

@interface WebPDFView : NSView <WebDocumentView, WebDocumentSearching>
{
    PDFView *PDFSubview;
    WebDataSource *dataSource;
    NSString *path;
    BOOL written;
}

+ (NSBundle *)PDFKitBundle;
- (PDFView *)PDFSubview;

@end

#endif  // OMIT_TIGER_FEATURES
