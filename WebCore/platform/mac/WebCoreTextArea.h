/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#import <Cocoa/Cocoa.h>

@class WebCoreTextView;
class QTextEdit;
@protocol WebCoreWidgetHolder;

@interface WebCoreTextArea : NSScrollView <WebCoreWidgetHolder>
{
    WebCoreTextView *textView;
    QTextEdit *widget;
    NSFont *_font;
    float _lineHeight;
    BOOL wrap;
    BOOL inNextValidKeyView;
    BOOL inDrawingMachinery;
    BOOL inInitWithFrame;
    BOOL resizableByUser;
    BOOL resizableByUserComputed;
    BOOL normalizeLineEndings;
}

- initWithQTextEdit:(QTextEdit *)w;
- (void)detachQTextEdit;

- (void)setAlignment:(NSTextAlignment)alignment;
- (void)setLineHeight:(float)lineHeight;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;

- (void)setEditable:(BOOL)flag;
- (BOOL)isEditable;

- (void)setEnabled:(BOOL)flag;
- (BOOL)isEnabled;

- (void)setFont:(NSFont *)font;

- (void)setTextColor:(NSColor *)color;
- (void)setBackgroundColor:(NSColor *)color;

- (void)setText:(NSString *)text;
- (NSString *)text;
- (NSString *)textWithHardLineBreaks;

- (void)setWordWrap:(BOOL)wrap;
- (BOOL)wordWrap;

- (void)selectAll;
- (void)setSelectedRange:(NSRange)aRange;
- (NSRange)selectedRange;
- (BOOL)hasSelection;

- (NSSize)sizeWithColumns:(int)columns rows:(int)rows;

// paragraph-oriented functions for the benefit of QTextEdit
- (void)setCursorPositionToIndex:(int)index inParagraph:(int)paragraph;
- (void)getCursorPositionAsIndex:(int *)index inParagraph:(int *)paragraph;

@end
