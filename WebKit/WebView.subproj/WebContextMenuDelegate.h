/*	
        WebContextMenuHandler.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

// These strings are keys into the element dictionary provided in 
// contextMenuItemsForElement.
extern NSString *WebContextMenuElementLinkURLKey;
extern NSString *WebContextMenuElementLinkLabelKey;
extern NSString *WebContextMenuElementImageURLKey;
extern NSString *WebContextMenuElementStringKey;
extern NSString *WebContextMenuElementImageKey;
extern NSString *WebContextMenuElementFrameKey;

/*!
    @protocol WebContextMenuHandler
    @discussion WebContextMenuHandler determine what context menu items are visible over
    a clicked element.
*/

@protocol WebContextMenuHandler <NSObject>

/*!
    @method contextMenuItemsForElement:defaultMenuItems:
    @discussion Returns the array of NSMenuItems that will be displayed in the context menu 
    for the dictionary representation of the clicked element.
*/
- (NSArray *)contextMenuItemsForElement: (NSDictionary *)element  defaultMenuItems: (NSArray *)menuItems;

@end



