//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebFileButton.h>
#import <WebKit/WebJavaScriptTextInputPanel.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebViewFactory.h>

#import <WebFoundation/WebAssertions.h>

@implementation WebViewFactory

+ (void)createSharedFactory;
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
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
