/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#import "ExtensionManagerWindowController.h"
#import "SettingsController.h"
#import "WK1BrowserWindowController.h"
#import "WK2BrowserWindowController.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserContentExtensionStore.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

enum {
    WebKit1NewWindowTag = 1,
    WebKit2NewWindowTag = 2,
    WebKit1NewEditorTag = 3,
    WebKit2NewEditorTag = 4
};

@implementation NSApplication (MiniBrowserApplicationExtensions)

- (BrowserAppDelegate *)browserAppDelegate
{
    return (BrowserAppDelegate *)[self delegate];
}

@end

@interface NSApplication (TouchBar)
@property (getter=isAutomaticCustomizeTouchBarMenuItemEnabled) BOOL automaticCustomizeTouchBarMenuItemEnabled;

@property (readonly, nonatomic) WKWebViewConfiguration *defaultConfiguration;

@end

@implementation BrowserAppDelegate

- (id)init
{
    self = [super init];
    if (self) {
        _browserWindowControllers = [[NSMutableSet alloc] init];
        _extensionManagerWindowController = [[ExtensionManagerWindowController alloc] init];
    }

    return self;
}

- (void)awakeFromNib
{
    _settingsController = [[SettingsController alloc] initWithMenu:_settingsMenu];

    if ([_settingsController usesGameControllerFramework])
        [WKProcessPool _forceGameControllerFramework];

//    [[NSApp mainMenu] insertItem:[item autorelease] atIndex:[[NSApp mainMenu] indexOfItemWithTitle:@"Debug"]];

    if ([NSApp respondsToSelector:@selector(setAutomaticCustomizeTouchBarMenuItemEnabled:)])
        [NSApp setAutomaticCustomizeTouchBarMenuItemEnabled:YES];
}

static WKWebsiteDataStore *persistentDataStore()
{
    static WKWebsiteDataStore *dataStore;

    if (!dataStore) {
        _WKWebsiteDataStoreConfiguration *configuration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
        configuration.networkCacheSpeculativeValidationEnabled = YES;
        dataStore = [[WKWebsiteDataStore alloc] _initWithConfiguration:configuration];
    }
    
    return dataStore;
}

- (WKWebViewConfiguration *)defaultConfiguration
{
    static WKWebViewConfiguration *configuration;

    if (!configuration) {
        configuration = [[WKWebViewConfiguration alloc] init];
        configuration.websiteDataStore = persistentDataStore();

        _WKProcessPoolConfiguration *processConfiguration = [[[_WKProcessPoolConfiguration alloc] init] autorelease];
        if (_settingsController.perWindowWebProcessesDisabled)
            processConfiguration.usesSingleWebProcess = YES;
        if (_settingsController.processSwapOnWindowOpenWithOpenerEnabled)
            processConfiguration.processSwapsOnWindowOpenWithOpener = true;
        
        configuration.processPool = [[[WKProcessPool alloc] _initWithConfiguration:processConfiguration] autorelease];

        NSArray<_WKExperimentalFeature *> *experimentalFeatures = [WKPreferences _experimentalFeatures];
        for (_WKExperimentalFeature *feature in experimentalFeatures) {
            BOOL enabled;
            if ([[NSUserDefaults standardUserDefaults] objectForKey:feature.key])
                enabled = [[NSUserDefaults standardUserDefaults] boolForKey:feature.key];
            else
                enabled = [feature defaultValue];
            [configuration.preferences _setEnabled:enabled forExperimentalFeature:feature];
        }

        NSArray<_WKInternalDebugFeature *> *internalDebugFeatures = [WKPreferences _internalDebugFeatures];
        for (_WKInternalDebugFeature *feature in internalDebugFeatures) {
            BOOL enabled;
            if ([[NSUserDefaults standardUserDefaults] objectForKey:feature.key])
                enabled = [[NSUserDefaults standardUserDefaults] boolForKey:feature.key];
            else
                enabled = [feature defaultValue];
            [configuration.preferences _setEnabled:enabled forInternalDebugFeature:feature];
        }

        configuration._mediaCaptureEnabled = YES;
        configuration.preferences._fullScreenEnabled = YES;
        configuration.preferences._allowsPictureInPictureMediaPlayback = YES;
        configuration.preferences._developerExtrasEnabled = YES;
        configuration.preferences._mockCaptureDevicesEnabled = YES;
        configuration.preferences._accessibilityIsolatedTreeEnabled = YES;
    }

    configuration.suppressesIncrementalRendering = _settingsController.incrementalRenderingSuppressed;
    configuration.websiteDataStore._resourceLoadStatisticsEnabled = _settingsController.resourceLoadStatisticsEnabled;
    return configuration;
}

- (WKPreferences *)defaultPreferences
{
    return self.defaultConfiguration.preferences;
}

- (BrowserWindowController *)createBrowserWindowController:(id)sender
{
    BrowserWindowController *controller = nil;
    BOOL useWebKit2 = NO;
    BOOL makeEditable = NO;

    if (![sender respondsToSelector:@selector(tag)]) {
        useWebKit2 = _settingsController.useWebKit2ByDefault;
        makeEditable = _settingsController.createEditorByDefault;
    } else {
        useWebKit2 = [sender tag] == WebKit2NewWindowTag || [sender tag] == WebKit2NewEditorTag;
        makeEditable = [sender tag] == WebKit1NewEditorTag || [sender tag] == WebKit2NewEditorTag;
    }

    if (!useWebKit2)
        controller = [[WK1BrowserWindowController alloc] initWithWindowNibName:@"BrowserWindow"];
    else
        controller = [[WK2BrowserWindowController alloc] initWithConfiguration:[self defaultConfiguration]];

    if (makeEditable)
        controller.editable = YES;

    if (!controller)
        return nil;

    [_browserWindowControllers addObject:controller];

    return controller;
}

- (IBAction)newWindow:(id)sender
{
    BrowserWindowController *controller = [self createBrowserWindowController:sender];
    if (!controller)
        return;

    [[controller window] makeKeyAndOrderFront:sender];
    [controller loadURLString:_settingsController.defaultURL];
}

- (IBAction)newPrivateWindow:(id)sender
{
    WKWebViewConfiguration *privateConfiguraton = [self.defaultConfiguration copy];
    privateConfiguraton.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    BrowserWindowController *controller = [[WK2BrowserWindowController alloc] initWithConfiguration:privateConfiguraton];
    [privateConfiguraton release];

    [[controller window] makeKeyAndOrderFront:sender];
    [_browserWindowControllers addObject:controller];

    [controller loadURLString:_settingsController.defaultURL];
}

- (IBAction)newEditorWindow:(id)sender
{
    BrowserWindowController *controller = [self createBrowserWindowController:sender];
    if (!controller)
        return;

    [[controller window] makeKeyAndOrderFront:sender];
    [controller loadHTMLString:@"<html><body></body></html>"];
}

- (void)browserWindowWillClose:(NSWindow *)window
{
    [_browserWindowControllers removeObject:window.windowController];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    WebHistory *webHistory = [[WebHistory alloc] init];
    [WebHistory setOptionalSharedHistory:webHistory];
    [webHistory release];

    [self _updateNewWindowKeyEquivalents];

    if (_settingsController.createEditorByDefault)
        [self newEditorWindow:self];
    else
        [self newWindow:self];
}

- (BrowserWindowController *)frontmostBrowserWindowController
{
    for (NSWindow* window in [NSApp windows]) {
        id delegate = [window delegate];

        if (![delegate isKindOfClass:[BrowserWindowController class]])
            continue;

        BrowserWindowController *controller = (BrowserWindowController *)delegate;
        assert([_browserWindowControllers containsObject:controller]);
        return controller;
    }

    return nil;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    BrowserWindowController *controller = [self createBrowserWindowController:nil];
    if (!controller)
        return NO;

    [controller.window makeKeyAndOrderFront:self];
    [controller loadURLString:[NSURL fileURLWithPath:filename].absoluteString];
    return YES;
}

- (IBAction)openDocument:(id)sender
{
    BrowserWindowController *browserWindowController = [self frontmostBrowserWindowController];

    if (browserWindowController) {
        NSOpenPanel *openPanel = [[NSOpenPanel openPanel] retain];
        [openPanel beginSheetModalForWindow:browserWindowController.window completionHandler:^(NSInteger result) {
            if (result != NSModalResponseOK)
                return;

            NSURL *url = [openPanel.URLs objectAtIndex:0];
            [browserWindowController loadURLString:[url absoluteString]];
        }];
        return;
    }

    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel beginWithCompletionHandler:^(NSInteger result) {
        if (result != NSModalResponseOK)
            return;

        BrowserWindowController *controller = [self createBrowserWindowController:nil];
        [controller.window makeKeyAndOrderFront:self];

        NSURL *url = [openPanel.URLs objectAtIndex:0];
        [controller loadURLString:[url absoluteString]];
    }];
}

- (void)didChangeSettings
{
    [self _updateNewWindowKeyEquivalents];

    // Let all of the BrowserWindowControllers know that a setting changed, so they can attempt to dynamically update.
    for (BrowserWindowController *browserWindowController in _browserWindowControllers)
        [browserWindowController didChangeSettings];
}

- (void)_updateNewWindowKeyEquivalents
{
    NSEventModifierFlags webKit1Flags = _settingsController.useWebKit2ByDefault ? NSEventModifierFlagOption : 0;
    NSEventModifierFlags webKit2Flags = _settingsController.useWebKit2ByDefault ? 0 : NSEventModifierFlagOption;

    NSString *normalWindowEquivalent = _settingsController.createEditorByDefault ? @"N" : @"n";
    NSString *editorEquivalent = _settingsController.createEditorByDefault ? @"n" : @"N";

    _newWebKit1WindowItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | webKit1Flags;
    _newWebKit2WindowItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | webKit2Flags;
    _newWebKit1EditorItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | webKit1Flags;
    _newWebKit2EditorItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | webKit2Flags;

    _newWebKit1WindowItem.keyEquivalent = normalWindowEquivalent;
    _newWebKit2WindowItem.keyEquivalent = normalWindowEquivalent;
    _newWebKit1EditorItem.keyEquivalent = editorEquivalent;
    _newWebKit2EditorItem.keyEquivalent = editorEquivalent;
}

- (IBAction)showExtensionsManager:(id)sender
{
    [_extensionManagerWindowController showWindow:sender];
}

- (WKUserContentController *)userContentContoller
{
    return self.defaultConfiguration.userContentController;
}

- (IBAction)fetchDefaultStoreWebsiteData:(id)sender
{
    [persistentDataStore() fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray *websiteDataRecords) {
        NSLog(@"did fetch default store website data %@.", websiteDataRecords);
    }];
}

- (IBAction)fetchAndClearDefaultStoreWebsiteData:(id)sender
{
    [persistentDataStore() fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray *websiteDataRecords) {
        [persistentDataStore() removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] forDataRecords:websiteDataRecords completionHandler:^{
            [persistentDataStore() fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] completionHandler:^(NSArray *websiteDataRecords) {
                NSLog(@"did clear default store website data, after clearing data is %@.", websiteDataRecords);
            }];
        }];
    }];
}

- (IBAction)clearDefaultStoreWebsiteData:(id)sender
{
    [persistentDataStore() removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        NSLog(@"Did clear default store website data.");
    }];
}

@end
