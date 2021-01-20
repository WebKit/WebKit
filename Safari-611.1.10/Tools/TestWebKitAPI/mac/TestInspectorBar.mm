/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TestInspectorBar.h"

#if PLATFORM(MAC)

#import <objc/runtime.h>
#import <wtf/WeakObjCPtr.h>

@implementation TestInspectorBarItemController {
    WeakObjCPtr<TestInspectorBar> _testInspectorBar;
}

- (instancetype)initWithInspectorBar:(TestInspectorBar *)inspectorBar
{
    if (self = [super initWithInspectorBar:inspectorBar]) {
        inspectorBar.itemController = self;
        _testInspectorBar = inspectorBar;
    }
    return self;
}

- (TestInspectorBar *)inspectorBar
{
    return _testInspectorBar.get().get();
}

- (void)updateSelectedAttributes
{
    // Older versions of AppKit only invoke synchronous NSTextInputClient methods, which immediately causes a debug
    // assertion in WebKit. To prevent this assertion, we suppress selection updates for versions of macOS where AppKit
    // only knows how to update selected attributes using synchronous text input client methods. That being said, the
    // inspector bar item controller still needs to set its _client pointer to the inspector bar's client; otherwise,
    // attempts to send @selector(changeAttributes:) when changing font attributes will become no-ops.
    object_setInstanceVariable(self, "_client", [_testInspectorBar client]);
}

@end

@implementation TestInspectorBar {
    WeakObjCPtr<TestInspectorBarItemController> _testItemController;
}

- (instancetype)initWithWebView:(WKWebView<NSInspectorBarClient> *)webView
{
    if (self = [super init]) {
        self.visible = YES;
        self.client = webView;
        [webView.window setInspectorBar:self];
    }
    return self;
}

+ (Class)standardItemControllerClass
{
    return TestInspectorBarItemController.class;
}

+ (NSArray<NSString *> *)standardTextItemIdentifiers
{
    static NSArray<NSString *> *identifiers = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        identifiers = @[
            NSInspectorBarFontFamilyItemIdentifier,
            NSInspectorBarFontSizeItemIdentifier,
            NSInspectorBarTextForegroundColorItemIdentifier,
            NSInspectorBarTextBackgroundColorItemIdentifier,
            NSInspectorBarFontStyleItemIdentifier
        ];
    });
    return identifiers;
}

- (void)_setStyleControlSelected:(BOOL)selected atIndex:(NSInteger)index
{
    NSSegmentedControl *styleControls = [_testItemController textStyleSwitches];
    styleControls.selectedSegment = index;
    [styleControls setSelected:selected forSegment:index];
    [_testItemController _fontStyleAction:styleControls];
}

- (void)chooseFontSize:(CGFloat)fontSize
{
    [_testItemController fontSizeItemWasClicked:@(fontSize)];
}

- (void)chooseFontFamily:(NSString *)fontFamily
{
    NSPopUpButton *fontFamilyPopup = [_testItemController fontFamilyPopup];
    [_testItemController menuNeedsUpdate:fontFamilyPopup.cell.menu];
    for (NSMenuItem *item in [fontFamilyPopup itemArray]) {
        if ([item.representedObject isKindOfClass:NSString.class] && [fontFamily isEqualToString:item.representedObject]) {
            [fontFamilyPopup selectItem:item];
            [_testItemController _fontPopupAction:fontFamilyPopup];
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

- (void)_chooseColor:(NSColor *)color inColorWell:(NSColorWell *)colorWell
{
    [colorWell setColor:color];
    [_testItemController _colorAction:colorWell];
}

- (void)chooseForegroundColor:(NSColor *)color
{
    [self _chooseColor:color inColorWell:[_testItemController foregroundColorWell]];
}

- (void)chooseBackgroundColor:(NSColor *)color
{
    [self _chooseColor:color inColorWell:[_testItemController backgroundColorWell]];
}

- (void)formatBold:(BOOL)bold
{
    [self _setStyleControlSelected:bold atIndex:0];
}

- (void)formatItalic:(BOOL)italic
{
    [self _setStyleControlSelected:italic atIndex:1];
}

- (void)formatUnderline:(BOOL)underline
{
    [self _setStyleControlSelected:underline atIndex:2];
}

- (TestInspectorBarItemController *)itemController
{
    return _testItemController.get().get();
}

- (void)setItemController:(TestInspectorBarItemController *)itemController
{
    _testItemController = itemController;
}

@end

#endif // PLATFORM(MAC)
