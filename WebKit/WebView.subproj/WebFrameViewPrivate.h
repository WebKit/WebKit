/*
    WebFrameView.h
    Copyright (C) 2005 Apple Computer, Inc. All rights reserved.
    Private header file.
*/

#import <WebKit/WebFrameView.h>

@interface WebFrameView (WebPrivate)

/*!
    @method canPrintHeadersAndFooters
    @abstract Tells whether this frame can print headers and footers
    @result YES if the frame can, no otherwise
*/
- (BOOL)canPrintHeadersAndFooters;

/*!
    @method printOperationWithPrintInfo
    @abstract Creates a print operation set up to print this frame
    @result A newly created print operation object
*/
- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo;

@end
