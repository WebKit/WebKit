/*
      WebDefaultContextMenuDelegate.m
      Copyright 2002, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultContextMenuDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>


#import <WebFoundation/WebLocalizableStrings.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>

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
            title = UI_STRING("Download Link to Disk", "Download Link to Disk context menu item");
            action = @selector(downloadLinkToDisk:);
            break;
        case WebMenuItemTagCopyLinkToClipboard:
            title = UI_STRING("Copy Link to Clipboard", "Copy Link to Clipboard context menu item");
            action = @selector(copyLinkToClipboard:);
            break;
        case WebMenuItemTagOpenImageInNewWindow:
            title = UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
            action = @selector(openImageInNewWindow:);
            break;
        case WebMenuItemTagDownloadImageToDisk:
            title = UI_STRING("Download Image To Disk", "Download Image To Disk context menu item");
            action = @selector(downloadImageToDisk:);
            break;
        case WebMenuItemTagCopyImageToClipboard:
            title = UI_STRING("Copy Image to Clipboard", "Copy Image to Clipboard context menu item");
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
        if([NSURLConnection canHandleRequest:[NSURLRequest requestWithURL:linkURL]]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenLinkInNewWindow]];
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagDownloadLinkToDisk]];
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopyLinkToClipboard]];
        }
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
            WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    
            if (webFrame != [[webFrame webView] mainFrame]) {
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
    [pasteboard _web_writeURL:[element objectForKey:WebElementLinkURLKey]
                     andTitle:[element objectForKey:WebElementLinkLabelKey]
                    withOwner:self];
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
    NSData *tiff = [[element objectForKey:WebElementImageKey] TIFFRepresentation];
    
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
    [pasteboard setData:tiff forType:NSTIFFPboardType];
}

- (void)openFrameInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    [self openNewWindowWithURL:[[webFrame dataSource] _URL] element:element];
}


@end
