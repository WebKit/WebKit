/*	
        IFWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#ifdef READY_FOR_PRIMETIME



/*
   ============================================================================= 
*/
@protocol IFContextMenuHandler
// Returns the array of menu items for this node that will be displayed in the context menu.
// Typically this would be implemented by returning the results of IFWebView defaultContextMenuItemsForNode:
// after making any desired changes or additions.
- (NSArray *)contextMenuItemsForNode: (IFDOMNode *);
@end



#endif

