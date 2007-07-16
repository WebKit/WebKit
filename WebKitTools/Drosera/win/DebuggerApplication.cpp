/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
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

#include "config.h"
#include "DebuggerApplication.h"

// ------------------------------------------------------------------------------------------------------------
// FIXME This whole file needs to be re-written for win. However I'm leaving the functions here so I know
// what still needs to be done.  Please ignore this code for now I have not looked at it closely.
// ------------------------------------------------------------------------------------------------------------

//#include <WebKit/WebCoreStatistics.h>
//void DebuggerApplication::applicationDidFinishLaunching(NSNotification* ) // Get's called after main and the app has been created from the Nibs (called before applicationDidFinishLaunching)
//{
//    WebCoreStatistics.setShouldPrintExceptions(true);   //WebKit

    // Adding functions to be associated with notifications?
    // Need some way to get/handle notifications (JS?)
//    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverLoaded:) name:WebScriptDebugServerDidLoadNotification object:nil];
//    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverLoaded:) name:WebScriptDebugServerQueryReplyNotification object:nil];
//    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(serverUnloaded:) name:WebScriptDebugServerWillUnloadNotification object:nil];
//    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:WebScriptDebugServerQueryNotification object:nil];

//#pragma mark -
//#pragma mark Server Detection Callbacks

void DebuggerApplication::serverLoaded()
{
    // Get the process you are debugging
//    int processId = [notification.userInfo(WebScriptDebugServerProcessIdentifierKey).(intValue);
    // make sure THIS is not the process
    //if (processId == [[NSProcessInfo processInfo] processIdentifier])
    //    return;

    //Get the server info from passed in notification?
    //check if info is in the server first
    m_knownServerNames->add(1, "Fake Server"); // setObject:info forKey:[notification object]];
}

void DebuggerApplication::serverUnloaded()
{
    // remove passed in server
    m_knownServerNames->remove(1);
}

//#pragma mark -
//#pragma mark Attach Panel Actions

void DebuggerApplication::attach(int sender)   // actually attach Drosera to the WebKit app.
{
    //Check that there are servers

    // get selected server
    unsigned int row = sender; //[[attachTable selectedRowIndexes] firstIndex];
    std::string key = m_knownServerNames->get(row);

//    // DebuggerDocument will release on close
//    DebuggerDocument *document = [[DebuggerDocument alloc] initWithServerName:key];
//    [document showWindow:sender];
}

//#pragma mark -
//#pragma mark Table View Delegate

// How will delegates work in C++?
//int numberOfRowsInTableView(NSTableView* tableView) // delegate. Returns number of rows in table. duh.
int DebuggerApplication::numberOfRowsInTableView() const
{
    return m_knownServerNames->size();
}

std::string DebuggerApplication::tableView()//(NSTableView* tableView, NSTableColumn* tableColumn, int row) // delegate. Called when table is displayed
{
    return "";
}

void DebuggerApplication::tableView(/*(NSTableView* tableView, ID cell, NSTableColumn* tableColumn,*/ int row) // delegate. Called when table is displayed, displays the icon in the col/row ?
{
    std::string key = m_knownServerNames->get(row);
//    NSMutableDictionary *info = [m_knownServerNames objectForKey:key];
//    string *processName = [info objectForKey:WebScriptDebugServerProcessNameKey];
//    NSImage *icon = [info objectForKey:@"icon"];

//    if (!icon) {
//        NSString *path = [[NSWorkspace sharedWorkspace] fullPathForApplication:processName];
//        if (path) icon = [[NSWorkspace sharedWorkspace] iconForFile:path];
//        if (!icon) icon = [[NSWorkspace sharedWorkspace] iconForFileType:@"app"];
//        if (icon) [info setObject:icon forKey:@"icon"];
//        [icon setScalesWhenResized:YES];
//        [icon setSize:NSMakeSize(32, 32)];
//    }

//    [cell setImage:icon];
//    [cell setTitle:processName];
}

