/*	
        WebContextMenuDelegate.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

/*!
    @protocol WebContextMenuDelegate
    @discussion WebContextMenuDelegate determine what context menu items are visible over
    a clicked element.
*/

/*!
    @enum WebMenuItemTag
    @discussion Each menu item in the default menu items array passed in
    contextMenuItemsForElement:defaultMenuItems: has its tag set to one of the WebMenuItemTags.
    When iterating through the default menu items array, use the tag to differentiate between them.
*/

enum {
    WebMenuItemTagOpenLinkInNewWindow=1,
    WebMenuItemTagDownloadLinkToDisk,
    WebMenuItemTagCopyLinkToClipboard,
    WebMenuItemTagOpenImageInNewWindow,
    WebMenuItemTagDownloadImageToDisk,
    WebMenuItemTagCopyImageToClipboard,
    WebMenuItemTagOpenFrameInNewWindow,
    WebMenuItemTagCopy
};

@protocol WebContextMenuDelegate <NSObject>

/*!
    @method contextMenuItemsForElement:defaultMenuItems:
    @discussion Returns the array of NSMenuItems that will be displayed in the context menu 
    for the dictionary representation of the clicked element.
*/
- (NSArray *)contextMenuItemsForElement:(NSDictionary *)element defaultMenuItems:(NSArray *)defaultMenuItems;

@end



