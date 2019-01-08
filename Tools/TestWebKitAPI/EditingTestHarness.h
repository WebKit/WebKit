/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if WK_API_ENABLED

#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>

@interface EditingTestHarness : NSObject<WKUIDelegatePrivate> {
    RetainPtr<NSMutableArray<NSDictionary *>> _editorStateHistory;
    RetainPtr<TestWKWebView> _webView;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView;

@property (nonatomic, readonly) TestWKWebView *webView;
@property (nonatomic, readonly) NSDictionary *latestEditorState;
@property (nonatomic, readonly) NSArray<NSDictionary *> *editorStateHistory;

- (void)insertParagraph;
- (void)insertText:(NSString *)text;
- (void)insertHTML:(NSString *)html;
- (void)selectAll;
- (void)deleteBackwards;
- (void)insertParagraphAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)insertText:(NSString *)text andExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)insertHTML:(NSString *)html andExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)selectAllAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)moveBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)moveWordBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)deleteBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)toggleBold;
- (void)toggleItalic;
- (void)toggleUnderline;
- (void)setForegroundColor:(NSString *)colorAsString;
- (void)alignJustifiedAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)alignLeftAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)alignCenterAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;
- (void)alignRightAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries;

- (BOOL)latestEditorStateContains:(NSDictionary<NSString *, id> *)entries;

@end

#endif // WK_API_ENABLED
