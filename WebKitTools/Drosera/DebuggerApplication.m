/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "DebuggerApplication.h"
#import "DebuggerDocument.h"
#import <WebKit/WebCoreStatistics.h>

@implementation DebuggerApplication
- (void)awakeFromNib
{
    NSTableColumn *column = [attachTable tableColumnWithIdentifier:@"name"];
    NSBrowserCell *cell = [[NSBrowserCell alloc] init];
    [cell setLeaf:YES];
    [column setDataCell:cell];
    [cell release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [WebCoreStatistics setShouldPrintExceptions:YES];

    knownServerNames = [[NSMutableDictionary alloc] init];

    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverLoaded:) name:WebScriptDebugServerDidLoadNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverLoaded:) name:WebScriptDebugServerQueryReplyNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverUnloaded:) name:WebScriptDebugServerWillUnloadNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerQueryNotification object:nil];

    [self showAttachPanel:nil];
}

#pragma mark -
#pragma mark Server Detection Callbacks

- (void)serverLoaded:(NSNotification *)notification
{
    int processId = [[[notification userInfo] objectForKey:WebScriptDebugServerProcessIdentifierKey] intValue];
    if (processId == [[NSProcessInfo processInfo] processIdentifier])
        return;

    NSMutableDictionary *info = [[notification userInfo] mutableCopy];
    if (!info)
        return;
    [knownServerNames setObject:info forKey:[notification object]];
    [info release];

    [attachTable reloadData];
}

- (void)serverUnloaded:(NSNotification *)notification
{
    [knownServerNames removeObjectForKey:[notification object]];
    [attachTable reloadData];
}

- (NSDictionary *)knownServers
{
    return knownServerNames;
}

#pragma mark -
#pragma mark Attach Panel Actions

- (IBAction)showAttachPanel:(id)sender
{
    if (![attachWindow isVisible])
        [attachWindow center];
    [attachTable reloadData];
    [attachTable setTarget:self];
    [attachTable setDoubleAction:@selector(attach:)];
    [attachWindow makeKeyAndOrderFront:sender];
}

- (IBAction)attach:(id)sender
{
    if ([[attachTable selectedRowIndexes] count] != 1)
        return;

    [attachWindow orderOut:sender];

    unsigned int row = [[attachTable selectedRowIndexes] firstIndex];
    NSString *key = [[knownServerNames allKeys] objectAtIndex:row];

    // DebuggerDocument will release on close
    DebuggerDocument *document = [[DebuggerDocument alloc] initWithServerName:key];
    [document showWindow:sender];
}

#pragma mark -
#pragma mark Table View Delegate

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [knownServerNames count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    return @"";
}

- (void)tableView:(NSTableView *)tableView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    NSString *key = [[knownServerNames allKeys] objectAtIndex:row];
    NSMutableDictionary *info = [knownServerNames objectForKey:key];
    NSString *processName = [info objectForKey:WebScriptDebugServerProcessNameKey];
    NSImage *icon = [info objectForKey:@"icon"];

    if (!icon) {
        NSString *path = [[NSWorkspace sharedWorkspace] fullPathForApplication:processName];
        if (path) icon = [[NSWorkspace sharedWorkspace] iconForFile:path];
        if (!icon) icon = [[NSWorkspace sharedWorkspace] iconForFileType:@"app"];
        if (icon) [info setObject:icon forKey:@"icon"];
        [icon setScalesWhenResized:YES];
        [icon setSize:NSMakeSize(32, 32)];
    }

    [cell setImage:icon];
    [cell setTitle:processName];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [attachButton setEnabled:([[attachTable selectedRowIndexes] count])];
}
@end
