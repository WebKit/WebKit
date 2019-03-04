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


#include "config.h"
#include "EditingTestHarness.h"

#import "PlatformUtilities.h"
#import <WebKit/WKWebViewPrivate.h>

@implementation EditingTestHarness

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        [_webView setUIDelegate:self];
        _editorStateHistory = adoptNS([[NSMutableArray alloc] init]);
    }
    return self;
}

- (void)dealloc
{
    if ([_webView UIDelegate] == self)
        [_webView setUIDelegate:nil];

    [super dealloc];
}

- (TestWKWebView *)webView
{
    return _webView.get();
}

- (NSDictionary *)latestEditorState
{
    return self.editorStateHistory.lastObject;
}

- (NSArray<NSDictionary *> *)editorStateHistory
{
    return _editorStateHistory.get();
}

- (void)insertParagraph
{
    [self insertParagraphAndExpectEditorStateWith:nil];
}

- (void)insertText:(NSString *)text
{
    [self insertText:text andExpectEditorStateWith:nil];
}

- (void)insertHTML:(NSString *)html
{
    [self insertHTML:html andExpectEditorStateWith:nil];
}

- (void)selectAll
{
    [self selectAllAndExpectEditorStateWith:nil];
}

- (void)deleteBackwards
{
    [self deleteBackwardAndExpectEditorStateWith:nil];
}

- (void)moveBackward
{
    [self moveBackwardAndExpectEditorStateWith:nil];
}

- (void)moveForward
{
    [self moveForwardAndExpectEditorStateWith:nil];
}

- (void)insertText:(NSString *)text andExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"InsertText" argument:text expectEntries:entries];
}

- (void)insertHTML:(NSString *)html andExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"InsertHTML" argument:html expectEntries:entries];
}

- (void)selectAllAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"SelectAll" argument:nil expectEntries:entries];
}

- (void)moveBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"MoveBackward" argument:nil expectEntries:entries];
}

- (void)moveWordBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"MoveWordBackward" argument:nil expectEntries:entries];
}

- (void)moveForwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"MoveForward" argument:nil expectEntries:entries];
}

- (void)toggleBold
{
    [self _execCommand:@"ToggleBold" argument:nil expectEntries:nil];
}

- (void)toggleItalic
{
    [self _execCommand:@"ToggleItalic" argument:nil expectEntries:nil];
}

- (void)toggleUnderline
{
    [self _execCommand:@"ToggleUnderline" argument:nil expectEntries:nil];
}

- (void)setForegroundColor:(NSString *)colorAsString
{
    [self _execCommand:@"ForeColor" argument:colorAsString expectEntries:nil];
}

- (void)alignJustifiedAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"AlignJustified" argument:nil expectEntries:entries];
}

- (void)alignLeftAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"AlignLeft" argument:nil expectEntries:entries];
}

- (void)alignCenterAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"AlignCenter" argument:nil expectEntries:entries];
}

- (void)alignRightAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"AlignRight" argument:nil expectEntries:entries];
}

- (void)insertParagraphAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"InsertParagraph" argument:nil expectEntries:entries];
}

- (void)deleteBackwardAndExpectEditorStateWith:(NSDictionary<NSString *, id> *)entries
{
    [self _execCommand:@"DeleteBackward" argument:nil expectEntries:entries];
}

- (void)_execCommand:(NSString *)command argument:(NSString *)argument expectEntries:(NSDictionary<NSString *, id> *)entries
{
    __block BOOL result = false;
    __block bool done = false;
    [_webView _executeEditCommand:command argument:argument completion:^(BOOL success) {
        result = success;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [_webView waitForNextPresentationUpdate];

    EXPECT_TRUE(result);
    if (!result)
        NSLog(@"Failed to execute editing command: ('%@', '%@')", command, argument ?: @"");

    BOOL containsEntries = [self latestEditorStateContains:entries];
    EXPECT_TRUE(containsEntries);
    if (!containsEntries)
        NSLog(@"Expected %@ to contain %@", self.latestEditorState, entries);
}

- (BOOL)latestEditorStateContains:(NSDictionary<NSString *, id> *)entries
{
    NSDictionary *latestEditorState = self.latestEditorState;
    for (NSString *key in entries) {
        if (![latestEditorState[key] isEqual:entries[key]])
            return NO;
    }
    return latestEditorState.count || !entries.count;
}

#pragma mark - WKUIDelegatePrivate

- (void)_webView:(WKWebView *)webView editorStateDidChange:(NSDictionary *)editorState
{
    if (![editorState[@"post-layout-data"] boolValue])
        return;

    if (![self.latestEditorState isEqualToDictionary:editorState])
        [_editorStateHistory addObject:editorState];
}

@end
