/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebDocumentPrivate.h>

/*!
@protocol _WebDocumentTextSizing
@discussion Optional protocol for making text larger and smaller.
*/
@protocol _WebDocumentTextSizing <NSObject>

// Methods to perform the actual commands
- (IBAction)_makeTextSmaller:(id)sender;
- (IBAction)_makeTextLarger:(id)sender;
- (IBAction)_makeTextStandardSize:(id)sender;

// Views that do text sizing come in two flavors.  Some will track the common textSizeMultiplier factor stored
// in the WebView.  Others (see PDFView) keep their own scaling factor, but still want to play along loosely
// with the smaller/larger commands, which in the user model operate across all frames of the WebView.
- (BOOL)_tracksCommonSizeFactor;

// Views that do not track the common size factor must answer for themselves if they are able to zoom in
// or out.  Views that do track it are not sent these messages.
- (BOOL)_canMakeTextSmaller;
- (BOOL)_canMakeTextLarger;
- (BOOL)_canMakeTextStandardSize;

@end

@protocol WebDocumentDragging <NSObject>
- (NSDragOperation)draggingUpdatedWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask;
- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask;
- (void)draggingCancelledWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo;
@end

@protocol WebDocumentElement <NSObject>
- (NSDictionary *)elementAtPoint:(NSPoint)point;
- (NSDictionary *)elementAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow;
@end

/* Used to save and restore state in the view, typically when going back/forward */
@protocol _WebDocumentViewState <NSObject>
- (NSPoint)scrollPoint;
- (void)setScrollPoint:(NSPoint)p;
- (id)viewState;
- (void)setViewState:(id)statePList;
@end
