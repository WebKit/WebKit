/*
      WebDefaultContextMenuDelegate.m
      Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultContextMenuDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebWindowOperationsDelegate.h>

@implementation WebDefaultContextMenuDelegate

- (void)dealloc
{
    [element release];
    [super dealloc];
}

+ (void)addMenuItemWithTitle:(NSString *)title action:(SEL)selector target:(id)target toArray:(NSMutableArray *)menuItems
{
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:@""];
    [menuItem setTarget:target];
    [menuItems addObject:menuItem];
    [menuItem release];
}

- (NSArray *)contextMenuItemsForElement: (NSDictionary *)theElement  defaultMenuItems: (NSArray *)defaultMenuItems
{
    NSMutableArray *menuItems = [NSMutableArray array];
    NSURL *linkURL, *imageURL;
    
    [element release];
    element = [theElement retain];

    linkURL = [element objectForKey:WebContextMenuElementLinkURLKey];

    if(linkURL){
    
        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Open Link in New Window", @"Open in New Window context menu item") 				                    action:@selector(openLinkInNewWindow:)
                                    target:self
                                   toArray:menuItems];
        
        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Download Link to Disk", @"Download Link to Disk context menu item") 				                    action:@selector(downloadLinkToDisk:)
                                    target:self
                                   toArray:menuItems];

        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Copy Link to Clipboard", @"Copy Link to Clipboard context menu item") 				                    action:@selector(copyLinkToClipboard:)
                                    target:self
                                   toArray:menuItems];
    }

    imageURL = [element objectForKey:WebContextMenuElementImageURLKey];

    if(imageURL){
        
        if(linkURL){
            [menuItems addObject:[NSMenuItem separatorItem]];
        }

        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Open Image in New Window", @"Open Image in New Window context menu item")
                                    action:@selector(openImageInNewWindow:)
                                    target:self
                                   toArray:menuItems];

        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Download Image To Disk", @"Download Image To Disk context menu item") 				                    action:@selector(downloadImageToDisk:)
                                    target:self
                                   toArray:menuItems];

        [[self class] addMenuItemWithTitle:NSLocalizedString(@"Copy Image to Clipboard", @"Copy Image to Clipboard context menu item") 				                    action:@selector(copyImageToClipboard:)
                                    target:self
                                   toArray:menuItems];
    }

    if(!imageURL && !linkURL){
    
        WebFrame *webFrame = [element objectForKey:WebContextMenuElementFrameKey];

        if(![[webFrame dataSource] isMainDocument]){
            [[self class] addMenuItemWithTitle:NSLocalizedString(@"Open Frame in New Window", @"Open Frame in New Window context menu item") 				                action:@selector(openFrameInNewWindow:)
                                        target:self
                                       toArray:menuItems];
        }
    }

    return menuItems;
}

- (void)openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer
{
    WebFrame *webFrame = [element objectForKey:WebContextMenuElementFrameKey];
    WebController *controller = [webFrame controller];
    [[controller windowOperationsDelegate] openNewWindowWithURL:URL referrer:referrer];
}

- (void)downloadURL:(NSURL *)URL
{
    WebFrame *webFrame = [element objectForKey:WebContextMenuElementFrameKey];
    WebController *controller = [webFrame controller];
    WebContentPolicy *contentPolicy = [[controller policyDelegate] contentPolicyForMIMEType:@"application/octet-stream" URL:URL inFrame:webFrame];
    [controller _downloadURL:URL toPath:[contentPolicy path]];
}

- (void)openLinkInNewWindow:(id)sender
{
    [self openNewWindowWithURL:[element objectForKey:WebContextMenuElementLinkURLKey] referrer:nil];
}

- (void)downloadLinkToDisk:(id)sender
{
    [self downloadURL:[element objectForKey:WebContextMenuElementLinkURLKey]];
}

- (void)copyLinkToClipboard:(id)sender
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard _web_writeURL:[element objectForKey:WebContextMenuElementLinkURLKey]
                     andTitle:[element objectForKey:WebContextMenuElementLinkLabelKey]
                    withOwner:self];
}

- (void)openImageInNewWindow:(id)sender
{
    [self openNewWindowWithURL:[element objectForKey:WebContextMenuElementImageURLKey] referrer:nil];
}

- (void)downloadImageToDisk:(id)sender
{
    [self downloadURL:[element objectForKey:WebContextMenuElementImageURLKey]];
}

- (void)copyImageToClipboard:(id)sender
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSData *tiff = [[element objectForKey:WebContextMenuElementImageKey] TIFFRepresentation];
    
    [pasteboard declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
    [pasteboard setData:tiff forType:NSTIFFPboardType];
}

- (void)openFrameInNewWindow:(id)sender
{
    WebFrame *webFrame = [element objectForKey:WebContextMenuElementFrameKey];
    WebDataSource *dataSource = [webFrame dataSource];
    NSURL *URL = [dataSource URL];
    [self openNewWindowWithURL:URL referrer:nil];
}


@end
