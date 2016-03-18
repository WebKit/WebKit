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

#import "CannedContent.h"
#import "EditingOperations.h"
#import "WK1WebDocumentController.h"
#import "WK2WebDocumentController.h"
#import <WebKit/WKBrowsingContextController.h>

static NSString * const UseWebKit2ByDefaultPreferenceKey = @"UseWebKit2ByDefault";

@implementation WebEditingAppDelegate {
    Class _openingDocumentController;
}

- (id)init
{
    self = [super init];
    if (self)
        _webDocuments = [[NSMutableSet alloc] init];
    
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [NSURLProtocol registerClass:[CannedContent class]];
    [WKBrowsingContextController registerSchemeForCustomProtocol:@"canned"];

    [self newEditor:self];
    [self _updateNewWindowKeyEquivalents];
}

- (IBAction)newEditor:(id)sender
{
    Class controllerClass;

    if (sender == self)
        controllerClass = self._defaultWebDocumentControllerClass;
    else if (sender == _newWebKit2EditorItem)
        controllerClass = [WK2WebDocumentController class];
    else if (sender == _newWebKit1EditorItem)
        controllerClass = [WK1WebDocumentController class];
    
    WebDocumentController *controller = [[controllerClass alloc] init];
    [[controller window] makeKeyAndOrderFront:sender];
    [_webDocuments addObject:controller];
    [controller loadHTMLString:[WebDocumentController defaultEditingSource]];
}

- (IBAction)showOperations:(id)sender
{
    static BOOL initialized = NO;

    if (!initialized) {
        NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSControlSizeMini]];
        NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
        NSArray *operations = editingOperations();
        
        float maxWidth = 0;
        for (NSString *operationName in operations)
            maxWidth = MAX(maxWidth, [operationName sizeWithAttributes:attributes].width);

        maxWidth += 24;
        
        unsigned long columnHeight = (operations.count + 2) / 3;
        
        NSView *superview = [_operationsPanel contentView];
        
        [_operationsPanel setContentSize:NSMakeSize(3 * maxWidth, columnHeight * 16 + 1)];
        
        float firstY = NSMaxY([superview frame]) - 1;
        float y = firstY;
        float x = 0;
        for (NSString *operationName in operations) {
            y -= 16;
            if (y < 0) {
                y = firstY - 16;
                x += maxWidth;
            }
            NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, maxWidth, 16)];
            [button setBezelStyle:NSRoundedBezelStyle];
            [button.cell setControlSize:NSControlSizeMini];
            [button setFont:font];
            [button setTitle:operationName];
            [button setAction:NSSelectorFromString(operationName)];
            [superview addSubview:button];
        }
        
        [_operationsPanel center];
        [_operationsPanel setFloatingPanel:YES];
        [_operationsPanel setBecomesKeyOnlyIfNeeded:YES];
        initialized = YES;
    }

    [_operationsPanel orderFront:nil];
}

- (Class)_defaultWebDocumentControllerClass
{
    return self.useWebKit2ByDefault ? [WK2WebDocumentController class] : [WK1WebDocumentController class];
}

- (BOOL)useWebKit2ByDefault
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:UseWebKit2ByDefaultPreferenceKey];
}

- (IBAction)toggleUseWebKit2ByDefault:(id)sender
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setBool:![defaults boolForKey:UseWebKit2ByDefaultPreferenceKey] forKey:UseWebKit2ByDefaultPreferenceKey];
    [self _updateNewWindowKeyEquivalents];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];
    
    if (action == @selector(toggleUseWebKit2ByDefault:))
        [menuItem setState:[self useWebKit2ByDefault] ? NSOnState : NSOffState];
    
    return YES;
}

- (void)_openDocumentWithWebKit1:(id)sender
{
    _openingDocumentController = [WK1WebDocumentController class];
}

- (void)_openDocumentWithWebKit2:(id)sender
{
    _openingDocumentController = [WK2WebDocumentController class];
}

- (IBAction)openDocument:(id)sender
{
    _openingDocumentController = self._defaultWebDocumentControllerClass;

    NSButtonCell *radioButtonPrototype = [[NSButtonCell alloc] init];
    radioButtonPrototype.buttonType = NSRadioButton;

    NSMatrix *openPanelAccessoryView = [[NSMatrix alloc] initWithFrame:NSZeroRect mode:NSRadioModeMatrix prototype:radioButtonPrototype numberOfRows:2 numberOfColumns:1];
    openPanelAccessoryView.autorecalculatesCellSize = YES;
    openPanelAccessoryView.autosizesCells = YES;
    NSArray *cells = openPanelAccessoryView.cells;
    [[cells objectAtIndex:0] setTitle:@"Open with WebKit1"];
    [[cells objectAtIndex:0] setAction:@selector(_openDocumentWithWebKit1:)];
    [[cells objectAtIndex:0] setState:!self.useWebKit2ByDefault];
    [[cells objectAtIndex:1] setTitle:@"Open with WebKit2"];
    [[cells objectAtIndex:1] setAction:@selector(_openDocumentWithWebKit2:)];
    [[cells objectAtIndex:1] setState:self.useWebKit2ByDefault];
    [openPanelAccessoryView sizeToFit];

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAccessoryView:openPanelAccessoryView];

    [panel beginWithCompletionHandler:^(NSInteger result) {
        if (result != NSFileHandlingPanelOKButton)
            return;

        NSURL *URL = panel.URLs.lastObject;
        [self application:[NSApplication sharedApplication] openFile:URL.path];
        _openingDocumentController = nil;
    }];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    Class controllerClass = _openingDocumentController ? _openingDocumentController : self._defaultWebDocumentControllerClass;
    WebDocumentController *controller = [[controllerClass alloc] init];
    [controller.window makeKeyAndOrderFront:nil];
    [_webDocuments addObject:controller];
    [controller loadHTMLString:[NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:nil]];
    return YES;
}

- (void)_updateNewWindowKeyEquivalents
{
    if (self.useWebKit2ByDefault) {
        [_newWebKit1EditorItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
        [_newWebKit2EditorItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    } else {
        [_newWebKit1EditorItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [_newWebKit2EditorItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    }
}

- (void)performTextFinderAction:(id)sender
{
    id keyWindowDelegate = [NSApplication sharedApplication].keyWindow.delegate;
    if (![keyWindowDelegate isKindOfClass:[WebDocumentController class]])
        return;
    [(WebDocumentController *)keyWindowDelegate performTextFinderAction:sender];
}

@end
