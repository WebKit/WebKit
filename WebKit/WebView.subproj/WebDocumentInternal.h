/*	
 WebDocumentInternal.h
 Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <Cocoa/Cocoa.h>

/*!
@protocol _web_WebDocumentTextSizing
@discussion Optional protocol for making text larger and smaller 
*/
@protocol _web_WebDocumentTextSizing <NSObject>

/*!
@method _web_textSizeMultiplierChanged
@abstract Called when the text size multiplier has been changed. -[WebView textSizeMultiplier] returns the current value.
*/
- (void)_web_textSizeMultiplierChanged;
@end
