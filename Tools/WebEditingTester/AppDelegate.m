/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "AppDelegate.h"

#import "WK1WebDocumentController.h"
#import "WK2WebDocumentController.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStore.h>

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";

@implementation WebEditingAppDelegate
- (id)init
{
    self = [super init];
    if (self)
        _webDocuments = [[NSMutableSet alloc] init];
    
    return self;
}

static WKWebViewConfiguration *defaultConfiguration()
{
    static WKWebViewConfiguration *configuration;
    
    if (!configuration) {
        configuration = [[WKWebViewConfiguration alloc] init];
        configuration.preferences._fullScreenEnabled = YES;
        configuration.preferences._developerExtrasEnabled = YES;
    }
    
    return configuration;
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self newEditor:self];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    for (WebDocumentController *documents in _webDocuments)
        [documents applicationTerminating];
}

- (IBAction)newEditor:(id)sender
{
    BOOL useWebKit2 = NO;

    if (sender == self)
        useWebKit2 = [self useWebKit2ByDefault];
    else if (sender == _newWK2EditorItem)
        useWebKit2 = YES;
    
    WebDocumentController *controller = nil;
    if (useWebKit2)
        controller = [[WK2WebDocumentController alloc] initWithConfiguration:defaultConfiguration()];
    else
        controller = [[WK1WebDocumentController alloc] initWithWindowNibName:@"WebDocument"];
    
    [[controller window] makeKeyAndOrderFront:sender];
    [_webDocuments addObject:controller];
    [controller loadContent];
}

- (IBAction)showOperations:(id)sender
{
    static BOOL initialized = NO;
    static NSString *operationNames[] = {
        @"alignCenter:",
        @"alignJustified:",
        @"alignLeft:",
        @"alignRight:",
        @"capitalizeWord:",
        @"centerSelectionInVisibleArea:",
        @"changeCaseOfLetter:",
        @"checkSpelling:",
        @"complete:",
        @"copy:",
        @"copyFont:",
        @"cut:",
        @"delete:",
        @"deleteBackward:",
        @"deleteBackwardByDecomposingPreviousCharacter:",
        @"deleteForward:",
        @"deleteToBeginningOfLine:",
        @"deleteToBeginningOfParagraph:",
        @"deleteToEndOfLine:",
        @"deleteToEndOfParagraph:",
        @"deleteToMark:",
        @"deleteWordBackward:",
        @"deleteWordForward:",
        @"ignoreSpelling:",
        @"indent:",
        @"insertBacktab:",
        @"insertLineBreak:",
        @"insertNewline:",
        @"insertNewlineIgnoringFieldEditor:",
        @"insertParagraphSeparator:",
        @"insertTab:",
        @"insertTabIgnoringFieldEditor:",
        @"insertTable:",
        @"lowercaseWord:",
        @"moveBackward:",
        @"moveBackwardAndModifySelection:",
        @"moveDown:",
        @"moveDownAndModifySelection:",
        @"moveForward:",
        @"moveForwardAndModifySelection:",
        @"moveLeft:",
        @"moveLeftAndModifySelection:",
        @"moveParagraphBackwardAndModifySelection:",
        @"moveParagraphForwardAndModifySelection:",
        @"moveRight:",
        @"moveRightAndModifySelection:",
        @"moveToBeginningOfDocument:",
        @"moveToBeginningOfDocumentAndModifySelection:",
        @"moveToBeginningOfSentence:",
        @"moveToBeginningOfSentenceAndModifySelection:",
        @"moveToBeginningOfLine:",
        @"moveToBeginningOfLineAndModifySelection:",
        @"moveToBeginningOfParagraph:",
        @"moveToBeginningOfParagraphAndModifySelection:",
        @"moveToEndOfDocument:",
        @"moveToEndOfDocumentAndModifySelection:",
        @"moveToEndOfSentence:",
        @"moveToEndOfSentenceAndModifySelection:",
        @"moveToEndOfLine:",
        @"moveToEndOfLineAndModifySelection:",
        @"moveToEndOfParagraph:",
        @"moveToEndOfParagraphAndModifySelection:",
        @"moveUp:",
        @"moveUpAndModifySelection:",
        @"moveWordBackward:",
        @"moveWordBackwardAndModifySelection:",
        @"moveWordForward:",
        @"moveWordForwardAndModifySelection:",
        @"moveWordLeft:",
        @"moveWordLeftAndModifySelection:",
        @"moveWordRight:",
        @"moveWordRightAndModifySelection:",
        @"outline:",
        @"pageDown:",
        @"pageDownAndModifySelection:",
        @"pageUp:",
        @"pageUpAndModifySelection:",
        @"paste:",
        @"pasteAsPlainText:",
        @"pasteAsRichText:",
        @"pasteFont:",
        @"scrollLineDown:",
        @"scrollLineUp:",
        @"scrollPageDown:",
        @"scrollPageUp:",
        @"selectAll:",
        @"selectSentence:",
        @"selectLine:",
        @"selectParagraph:",
        @"selectToMark:",
        @"selectWord:",
        @"setMark:",
        @"showGuessPanel:",
        @"startSpeaking:",
        @"stopSpeaking:",
        @"subscript:",
        @"superscript:",
        @"swapWithMark:",
        @"takeFindStringFromSelection:",
        @"toggleContinuousSpellChecking:",
        @"toggleSmartInsertDelete:",
        @"transpose:",
        @"transposeWords:",
        @"underline:",
        @"unscript:",
        @"uppercaseWord:",
        @"yank:",
        @"yankAndSelect:",
        NULL
    };

    if (!initialized) {
        NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]];
        NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
        
        float maxWidth = 0;
        int i;
        for (i = 0; operationNames[i]; ++i)
            maxWidth = MAX(maxWidth, [operationNames[i] sizeWithAttributes:attributes].width);

        maxWidth += 24;
        
        int columnHeight = (i + 2) / 3;
        
        NSView *superview = [_operationsPanel contentView];
        
        [_operationsPanel setContentSize:NSMakeSize(3 * maxWidth, columnHeight * 16 + 1)];
        
        float firstY = NSMaxY([superview frame]) - 1;
        float y = firstY;
        float x = 0;
        for (i = 0; operationNames[i]; ++i) {
            y -= 16;
            if (y < 0) {
                y = firstY - 16;
                x += maxWidth;
            }
            NSButton *button = [[[NSButton alloc] initWithFrame:NSMakeRect(x, y, maxWidth, 16)] autorelease];
            [button setBezelStyle:NSRoundedBezelStyle];
            [button.cell setControlSize:NSMiniControlSize];
            [button setFont:font];
            [button setTitle:operationNames[i]];
            [button setAction:NSSelectorFromString(operationNames[i])];
            [superview addSubview:button];
        }
        
        [_operationsPanel center];
        [_operationsPanel setFloatingPanel:YES];
        [_operationsPanel setBecomesKeyOnlyIfNeeded:YES];
        initialized = YES;
    }

    [_operationsPanel orderFront:nil];
}

- (BOOL)useWebKit2ByDefault
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseWebKit2ByDefaultPreferenceKey];
}

- (IBAction)toggleUseWK2ByDefault:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:![defaults boolForKey:UseWebKit2ByDefaultPreferenceKey] forKey:UseWebKit2ByDefaultPreferenceKey];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];
    
    if (action == @selector(toggleUseWK2ByDefault:))
        [menuItem setState:[self useWebKit2ByDefault] ? NSOnState : NSOffState];
    
    return YES;
}

@end
