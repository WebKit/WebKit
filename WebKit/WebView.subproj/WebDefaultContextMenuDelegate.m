/*
      WebDefaultContextMenuDelegate.m
      Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDefaultContextMenuDelegate.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/DOM.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDOMOperations.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebCore/WebCoreBridge.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>

@implementation WebDefaultUIDelegate (WebContextMenu)

- (NSMenuItem *)menuItemWithTag:(int)tag
{
    NSMenuItem *menuItem = [[NSMenuItem alloc] init];
    [menuItem setTarget:self];
    [menuItem setTag:tag];
    
    NSString *title;
    SEL action;
    
    switch(tag) {
        case WebMenuItemTagOpenLinkInNewWindow:
            title = UI_STRING("Open Link in New Window", "Open in New Window context menu item");
            action = @selector(openLinkInNewWindow:);
            break;
        case WebMenuItemTagDownloadLinkToDisk:
            title = UI_STRING("Download Linked File", "Download Linked File context menu item");
            action = @selector(downloadLinkToDisk:);
            break;
        case WebMenuItemTagCopyLinkToClipboard:
            title = UI_STRING("Copy Link", "Copy Link context menu item");
            action = @selector(copyLinkToClipboard:);
            break;
        case WebMenuItemTagOpenImageInNewWindow:
            title = UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
            action = @selector(openImageInNewWindow:);
            break;
        case WebMenuItemTagDownloadImageToDisk:
            title = UI_STRING("Download Image", "Download Image context menu item");
            action = @selector(downloadImageToDisk:);
            break;
        case WebMenuItemTagCopyImageToClipboard:
            title = UI_STRING("Copy Image", "Copy Image context menu item");
            action = @selector(copyImageToClipboard:);
            break;
        case WebMenuItemTagOpenFrameInNewWindow:
            title = UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
            action = @selector(openFrameInNewWindow:);
            break;
        case WebMenuItemTagCopy:
            title = UI_STRING("Copy", "Copy context menu item");
            action = @selector(copy:);
            [menuItem setTarget:nil];
            break;
        case WebMenuItemTagGoBack:
            title = UI_STRING("Back", "Back context menu item");
            action = @selector(goBack:);
            [menuItem setTarget:nil];
            break;
        case WebMenuItemTagGoForward:
            title = UI_STRING("Forward", "Forward context menu item");
            action = @selector(goForward:);
            [menuItem setTarget:nil];
            break;
        case WebMenuItemTagStop:
            title = UI_STRING("Stop", "Stop context menu item");
            action = @selector(stopLoading:);
            [menuItem setTarget:nil];
            break;
        case WebMenuItemTagReload:
            title = UI_STRING("Reload", "Reload context menu item");
            action = @selector(reload:);
            [menuItem setTarget:nil];
            break;
        default:
            [menuItem release];
            return nil;
    }

    [menuItem setTitle:title];
    [menuItem setAction:action];
    
    return [menuItem autorelease];
}

- (NSArray *)webView: (WebView *)wv contextMenuItemsForElement: (NSDictionary *)element  defaultMenuItems: (NSArray *)defaultMenuItems
{
    NSMutableArray *menuItems = [NSMutableArray array];
    
    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    
    if (linkURL) {
        if([WebView _canHandleRequest:[NSURLRequest requestWithURL:linkURL]]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenLinkInNewWindow]];
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagDownloadLinkToDisk]];
        }
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopyLinkToClipboard]];
    }
    
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    if (imageURL) {
        if (linkURL) {
            [menuItems addObject:[NSMenuItem separatorItem]];
        }
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenImageInNewWindow]];
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagDownloadImageToDisk]];
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopyImageToClipboard]];
    }
    
    if (!imageURL && !linkURL) {
        if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopy]];
        } else {
            if ([wv canGoBack]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagGoBack]];
            }
            if ([wv canGoForward]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagGoForward]];
            }
            if ([wv isLoading]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagStop]];
            } else {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagReload]];
            }
            
            WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
            if (webFrame != [wv mainFrame]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenFrameInNewWindow]];
            }
        }
    }
    
    // Attach element as the represented object to each item.
    [menuItems makeObjectsPerformSelector:@selector(setRepresentedObject:) withObject:element];
    
    return menuItems;
}

- (void)openNewWindowWithURL:(NSURL *)URL element:(NSDictionary *)element
{
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    WebView *webView = [webFrame webView];
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
    NSString *referrer = [[webFrame _bridge] referrer];
    if (referrer) {
	[request setHTTPReferrer:referrer];
    }
    
    [webView _openNewWindowWithRequest:request];
}

- (void)downloadURL:(NSURL *)URL element:(NSDictionary *)element
{
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    WebView *webView = [webFrame webView];
    [webView _downloadURL:URL];
}

- (void)openLinkInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self openNewWindowWithURL:[element objectForKey:WebElementLinkURLKey] element:element];
}

- (void)downloadLinkToDisk:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self downloadURL:[element objectForKey:WebElementLinkURLKey] element:element];
}

- (void)copyLinkToClipboard:(id)sender
{
    NSDictionary *element = [sender representedObject];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSPasteboard _web_writableTypesForURL];
    [pasteboard declareTypes:types owner:self];    
    [[[element objectForKey:WebElementFrameKey] webView] _writeLinkElement:element 
                                                       withPasteboardTypes:types
                                                              toPasteboard:pasteboard];
}

- (void)openImageInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self openNewWindowWithURL:[element objectForKey:WebElementImageURLKey] element:element];
}

- (void)downloadImageToDisk:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self downloadURL:[element objectForKey:WebElementImageURLKey] element:element];
}

- (void)copyImageToClipboard:(id)sender
{
    NSDictionary *element = [sender representedObject];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSPasteboard _web_writableTypesForImage];
    [pasteboard declareTypes:types owner:self];
    [[[element objectForKey:WebElementFrameKey] webView] _writeImageElement:element 
                                                        withPasteboardTypes:types 
                                                               toPasteboard:pasteboard];
}

- (void)openFrameInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    [self openNewWindowWithURL:[[webFrame dataSource] _URL] element:element];
}

@end
