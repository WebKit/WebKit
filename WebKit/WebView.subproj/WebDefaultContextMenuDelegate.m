/*
      WebDefaultContextMenuHandler.h

      Copyright 2002, Apple, Inc. All rights reserved.

*/

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultContextMenuHandler.h>
#import <WebKit/WebControllerPolicyHandler.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebFrame.h>

@implementation WebDefaultContextMenuHandler

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

    linkURL = [element objectForKey:WebContextLinkURL];

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

    imageURL = [element objectForKey:WebContextImageURL];

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
    
        WebFrame *webFrame = [element objectForKey:WebContextFrame];

        if(![[webFrame dataSource] isMainDocument]){
            [[self class] addMenuItemWithTitle:NSLocalizedString(@"Open Frame in New Window", @"Open Frame in New Window context menu item") 				                action:@selector(openFrameInNewWindow:)
                                        target:self
                                       toArray:menuItems];
        }
    }

    return menuItems;
}

- (void)openNewWindowWithURL:(NSURL *)URL
{
    WebFrame *webFrame = [element objectForKey:WebContextFrame];
    WebController *controller = [webFrame controller];
    [[controller windowContext] openNewWindowWithURL:URL];
}

- (void)downloadURL:(NSURL *)URL
{
    WebFrame *webFrame = [element objectForKey:WebContextFrame];
    WebController *controller = [webFrame controller];
    WebDataSource *dataSource = [[WebDataSource alloc] initWithURL:URL];

    // FIXME: This is a hack
    WebContentPolicy *contentPolicy = [[controller policyHandler] contentPolicyForMIMEType:@"application/octet-stream" dataSource:dataSource];
    [controller _downloadURL:URL toPath:[contentPolicy path]];
}

- (void)openLinkInNewWindow:(id)sender
{
    [self openNewWindowWithURL:[element objectForKey:WebContextLinkURL]];
}

- (void)downloadLinkToDisk:(id)sender
{
    [self downloadURL:[element objectForKey:WebContextLinkURL]];
}

- (void)copyLinkToClipboard:(id)sender
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSURL *URL = [element objectForKey:WebContextLinkURL];
    
    [pasteboard declareTypes:[NSArray arrayWithObjects:NSURLPboardType, NSStringPboardType, nil] owner:nil];
    [pasteboard setString:[URL absoluteString] forType:NSStringPboardType];
    [URL writeToPasteboard:pasteboard];
}

- (void)openImageInNewWindow:(id)sender
{
    [self openNewWindowWithURL:[element objectForKey:WebContextImageURL]];
}

- (void)downloadImageToDisk:(id)sender
{
    [self downloadURL:[element objectForKey:WebContextImageURL]];
}

- (void)copyImageToClipboard:(id)sender
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSData *tiff = [[element objectForKey:WebContextImage] TIFFRepresentation];
    
    [pasteboard declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
    [pasteboard setData:tiff forType:NSTIFFPboardType];
}

- (void)openFrameInNewWindow:(id)sender
{
    WebFrame *webFrame = [element objectForKey:WebContextFrame];
    WebDataSource *dataSource = [webFrame dataSource];
    NSURL *URL = [dataSource URL];
    [self openNewWindowWithURL:URL];
}


@end
