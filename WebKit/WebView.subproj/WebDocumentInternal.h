/*	
 WebDocumentInternal.h
 Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebDocumentPrivate.h>

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

@protocol WebDocumentDragging <NSObject>
- (NSDragOperation)draggingUpdatedWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask;
- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask;
- (void)draggingCancelledWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo;
@end

@protocol WebDocumentElement <NSObject>
- (NSDictionary *)elementAtPoint:(NSPoint)point;
@end

@protocol WebDocumentSelection <NSObject>
- (NSArray *)pasteboardTypesForSelection;
- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard;
@end
