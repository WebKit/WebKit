//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebViewFactory.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebLocalizableStrings.h>

#import <WebKit/WebFileButton.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebPluginDatabase.h>

@implementation WebViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message
{
    NSRunAlertPanel(UI_STRING("JavaScript", "title of JavaScript alert panel"), @"%@", nil, nil, nil, message);
}

- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    return NSRunAlertPanel(UI_STRING("JavaScript", "title of JavaScript alert panel"), @"%@", nil,
        UI_STRING("Cancel", "standard button label"), nil, message) == NSAlertDefaultReturn;
}

- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result
{
    WebJavaScriptTextInputPanel *panel = [[WebJavaScriptTextInputPanel alloc] initWithPrompt:prompt text:defaultText];
    [panel showWindow:nil];
    BOOL OK = [NSApp runModalForWindow:[panel window]];
    if (OK) {
        *result = [panel text];
    }
    [panel release];
    return OK;
}

- (NSView <WebCoreFileButton> *)fileButton
{
    return [[WebFileButton alloc] init];
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

@end
