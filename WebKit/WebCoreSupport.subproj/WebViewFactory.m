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
    [[panel window] close];
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

- (NSString *)inputElementAltText
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

- (NSString *)resetButtonDefaultLabel
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

- (NSString *)searchableIndexIntroduction
{
    return UI_STRING("This is a searchable index. Enter search keywords: ",
        "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

- (NSString *)submitButtonDefaultLabel
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

- (NSString *)defaultLanguageCode
{
    // FIXME: Need implementation. Defaults gives us a list of languages, but by name, not code.
    return @"en";
}

@end
