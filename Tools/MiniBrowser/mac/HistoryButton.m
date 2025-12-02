/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "HistoryButton.h"

#import "WK2BrowserWindowController.h"
#import <WebKit/WKBackForwardListPrivate.h>

@implementation HistoryButton {
    NSTimer *_holdTimer;
    NSEvent *_mouseDownEvent;
}

- (void)mouseDown:(NSEvent *)event
{
    if (self->_mouseDownEvent)
        return;

    [self highlight:YES];
    self->_mouseDownEvent = event;
    self->_holdTimer = [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:NO block:^(NSTimer *timer) {
        [self _showMenuTimerFired];
    }];
}

- (void)mouseUp:(NSEvent *)event
{
    if (!self->_mouseDownEvent)
        return;

    [self _cleanup];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [self.target performSelector:self.action withObject:self];
#pragma clang diagnostic pop
}

- (void)_cleanup
{
    [self->_holdTimer invalidate];
    self->_holdTimer = nil;
    [self highlight:NO];
    self->_mouseDownEvent = nil;
}

- (void)_itemSelected:(NSMenuItem *)item
{
    [((WK2BrowserWindowController *)self.target).webView goToBackForwardListItem:item.representedObject];
}

- (void)_showMenuTimerFired
{
    WKBackForwardList *backForwardList = ((WK2BrowserWindowController *)self.target).webView.backForwardList;
    NSArray<WKBackForwardListItem *> *items;
    NSString *menuTitle;
    BOOL pruned = NO;
    if (self.action == @selector(goBack:)) {
        items = [[backForwardList.backList reverseObjectEnumerator] allObjects];
        menuTitle = @"Back list";
    } else {
        items = backForwardList.forwardList;
        menuTitle = @"Forward list";
    }

    if (items.count > 15) {
        pruned = YES;
        items = [items subarrayWithRange:NSMakeRange(0, 15)];
    }

    NSMenu *menu = [[NSMenu alloc] initWithTitle:menuTitle];
    for (WKBackForwardListItem *item in items) {
        NSString *title = item.title;
        if (title.length > 0)
            title = [title stringByAppendingFormat:@" (%@)", item.URL.absoluteString];
        else
            title = item.URL.absoluteString;

        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title action:@selector(_itemSelected:) keyEquivalent:@""];
        menuItem.representedObject = item;
        [menu addItem:menuItem];
    }

    if (pruned) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:@"<pruned>" action:nil keyEquivalent:@""];
        menuItem.enabled = NO;
        [menu addItem:menuItem];
    }

    if (self->_mouseDownEvent.modifierFlags & NSEventModifierFlagOption)
        NSLog(@"Showing back/forward menu:%@", backForwardList._loggingStringForTesting);

    [NSMenu popUpContextMenu:menu withEvent:self->_mouseDownEvent forView:self];
    [self _cleanup];
}

@end
