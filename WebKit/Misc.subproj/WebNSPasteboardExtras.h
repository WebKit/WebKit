//
//  WebNSPasteboardExtras.h
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

extern NSString *WebURLPboardType;
extern NSString *WebURLNamePboardType;

@interface NSPasteboard (WebExtras)

// Returns the array of drag types that _web_writeURL:andTitle:withOwner: handles.
// FIXME: It would be better to conceal this completely by changing
// _web_writeURL:andTitle:withOwner:types: to take a list of additional types
// instead of a complete list of types, but we're afraid to do so at this late
// stage in Panther just in case some internal client is using _web_writeURL:andTitle:withOwner:types:
// already and relying on its current behavior.
+ (NSArray *)_web_writableDragTypesForURL;

// Returns the array of drag types that _web_bestURL handles; note that the presence
// of one or more of these drag types on the pasteboard is not a guarantee that
// _web_bestURL will return a non-nil result.
+ (NSArray *)_web_dragTypesForURL;

// Finds the best URL from the data on the pasteboard, giving priority to http and https URLs
-(NSURL *)_web_bestURL;

// Writes the URL to the pasteboard in all the types from _web_writableDragTypesForURL
- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title withOwner:(id)owner;

// Writes the URL to the pasteboard in all the types from _web_writableDragTypesForURL,
// after declaring all the passed-in types. Any passed-in types not in _web_writableDragTypesForURL
// must be handled by the caller separately.
- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title withOwner:(id)owner types:(NSArray *)types;

// Sets the text on the NSFindPboard. Returns the new changeCount for the NSFindPboard.
+ (int)_web_setFindPasteboardString:(NSString *)string withOwner:(id)owner;

@end
