/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

@class ExtensionManagerWindowController;
@class SettingsController;

@interface BrowserAppDelegate : NSObject <NSApplicationDelegate> {
    NSMutableSet *_browserWindowControllers;
    SettingsController *_settingsController;
    ExtensionManagerWindowController *_extensionManagerWindowController;

    IBOutlet NSMenu *_settingsMenu;

    IBOutlet NSMenuItem *_newWebKit1WindowItem;
    IBOutlet NSMenuItem *_newWebKit2WindowItem;
    IBOutlet NSMenuItem *_newWebKit1EditorItem;
    IBOutlet NSMenuItem *_newWebKit2EditorItem;

    bool _openNewWindowAtStartup;
}

- (void)browserWindowWillClose:(NSWindow *)window;

- (void)didChangeSettings;

@property (readonly, nonatomic) WKUserContentController *userContentContoller;
@property (readonly, nonatomic) SettingsController *settingsController;
@property (readonly, nonatomic) WKPreferences *defaultPreferences;

@end

@interface NSApplication (MiniBrowserApplicationExtensions)

- (BrowserAppDelegate *)browserAppDelegate;

@end
