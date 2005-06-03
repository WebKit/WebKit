//
//  WebViewFactory.m
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebViewFactory.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSUserDefaultsExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>

@interface NSMenu (WebViewFactoryAdditions)
- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag;
@end

@implementation NSMenu (WebViewFactoryAdditions)

- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag
{
    NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""] autorelease];
    [item setTag:tag];
    [self addItem:item];
    return item;
}

@end

@implementation WebViewFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

- (void)refreshPlugins:(BOOL)reloadPages
{
    [[WebPluginDatabase installedPlugins] refresh];
    if (reloadPages) {
        [WebViewSets makeWebViewsPerformSelector:@selector(_reloadForPluginChanges)];
    }
}

- (WebCoreBridge *)bridgeForView:(NSView *)v
{
    NSView *aView = [v superview];
    
    while (aView) {
        if ([aView isKindOfClass:[WebHTMLView class]]) {
            return [(WebHTMLView *)aView _bridge];
        }
        aView = [aView superview];
    }
    return nil;
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

- (NSMenu *)cellMenuForSearchField
{
    NSMenu* cellMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    [cellMenu addItemWithTitle:UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title")
                        action:NULL tag:NSSearchFieldRecentsTitleMenuItemTag];
    [cellMenu addItemWithTitle:@"" action:NULL tag:NSSearchFieldRecentsMenuItemTag];
    NSMenuItem *separator = [NSMenuItem separatorItem];
    [separator setTag:NSSearchFieldRecentsTitleMenuItemTag];
    [cellMenu addItem:separator];
    [cellMenu addItemWithTitle:UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents")
                        action:NULL tag:NSSearchFieldClearRecentsMenuItemTag];
    [cellMenu addItemWithTitle:UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed")
                        action:NULL tag:NSSearchFieldNoRecentsMenuItemTag];
    return cellMenu;
}

- (NSString *)defaultLanguageCode
{
    return [NSUserDefaults _webkit_preferredLanguageCode];
}

@end
