/*
      WebDefaultContextMenuHandler.h

      Copyright 2002, Apple, Inc. All rights reserved.

*/

#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultContextMenuHandler.h>
#import <WebKit/WebFrame.h>

@implementation WebDefaultContextMenuHandler

- (void)dealloc
{
    [currentInfo release];
    [super dealloc];
}

- (void)addMenuItemWithTitle:(NSString *)title action:(SEL)selector toArray:(NSMutableArray *)menuItems
{
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:@""];
    [menuItem setTarget:self];
    [menuItems addObject:menuItem];
    [menuItem release];
}

- (NSArray *)contextMenuItemsForElementInfo: (NSDictionary *)elementInfo  defaultMenuItems: (NSArray *)defaultMenuItems
{
    NSMutableArray *menuItems = [NSMutableArray array];
    NSURL *linkURL, *imageURL;
    
    currentInfo = [elementInfo retain];

    linkURL = [currentInfo objectForKey:WebContextLinkURL];

    if(linkURL){
        [self addMenuItemWithTitle:NSLocalizedString(@"Open Link in New Window", @"Open in New Window context menu item") 				            action:@selector(openInNewWindow:)
                           toArray:menuItems];
        
        [self addMenuItemWithTitle:NSLocalizedString(@"Download Link to Disk", @"Download Link to Disk context menu item") 				    action:@selector(downloadLinkToDisk:)
                           toArray:menuItems];

        [self addMenuItemWithTitle:NSLocalizedString(@"Copy Link to Clipboard", @"Copy Link to Clipboard context menu item") 				    action:@selector(copyLinkToClipboard:)
                           toArray:menuItems];

        [self addMenuItemWithTitle:NSLocalizedString(@"Add Link to Bookmarks", @"Add Link to Bookmarks context menu item") 				    action:@selector(addLinkToBookmarks:)
                           toArray:menuItems];
    }

    imageURL = [currentInfo objectForKey:WebContextImageURL];

    if(imageURL){
        if(linkURL){
            [menuItems addObject:[NSMenuItem separatorItem]];
        }
        [self addMenuItemWithTitle:NSLocalizedString(@"Open Image in New Window", @"Open Image in New Window context menu item") 		            action:@selector(openImageInNewWindow:)
                           toArray:menuItems];
        
        [self addMenuItemWithTitle:NSLocalizedString(@"Download Image To Disk", @"Download Image To Disk context menu item") 				    action:@selector(downloadImageToDisk:)
                           toArray:menuItems];
        
        [self addMenuItemWithTitle:NSLocalizedString(@"Copy Image to Clipboard", @"Copy Image to Clipboard context menu item") 				    action:@selector(copyImageToClipboard:)
                           toArray:menuItems];
        
        [self addMenuItemWithTitle:NSLocalizedString(@"Reload Image", @"Reload Image context menu item") 				                    action:@selector(reloadImage:)
                           toArray:menuItems];
    }

    if(!imageURL && !linkURL){
        WebFrame *webFrame = [currentInfo objectForKey:WebContextFrame];

        if([[webFrame dataSource] isMainDocument]){
            [self addMenuItemWithTitle:NSLocalizedString(@"View Source", @"View Source context menu item") 				                        action:@selector(viewSource:)
                               toArray:menuItems];
            
            [self addMenuItemWithTitle:NSLocalizedString(@"Save Page", @"Save Page context menu item")
                                action:@selector(savePage:)
                               toArray:menuItems];
        }else{
            [self addMenuItemWithTitle:NSLocalizedString(@"Open Frame in New Window", @"Open Frame in New Window context menu item") 				action:@selector(openFrameInNewWindow:)
                               toArray:menuItems];
            
            [self addMenuItemWithTitle:NSLocalizedString(@"View Frame Source", @"View Frame Source context menu item") 				                action:@selector(viewSource:)
                               toArray:menuItems];
            
            [self addMenuItemWithTitle:NSLocalizedString(@"Save Frame", @"Save Frame context menu item") 				                        action:@selector(savePage:)
                               toArray:menuItems];
        }
    }

    return menuItems;
}

@end
