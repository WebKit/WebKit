//
//  WebNSPasteboardExtras.h
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebArchive;
@class WebImageRenderer;

extern NSString *WebURLPboardType;
extern NSString *WebURLNamePboardType;

@interface NSPasteboard (WebExtras)

// Returns the array of types that _web_writeURL:andTitle: handles.
+ (NSArray *)_web_writableTypesForURL;

// Returns the array of types that _web_writeImage::::: handles.
+ (NSArray *)_web_writableTypesForImageIncludingArchive:(BOOL)hasArchive;

// Returns the array of drag types that _web_bestURL handles; note that the presence
// of one or more of these drag types on the pasteboard is not a guarantee that
// _web_bestURL will return a non-nil result.
+ (NSArray *)_web_dragTypesForURL;

// Finds the best URL from the data on the pasteboard, giving priority to http and https URLs
- (NSURL *)_web_bestURL;

// Writes the URL to the pasteboard with the passed types.
- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title types:(NSArray *)types;

// Sets the text on the NSFindPboard. Returns the new changeCount for the NSFindPboard.
+ (int)_web_setFindPasteboardString:(NSString *)string withOwner:(id)owner;

// Writes a file wrapper to the pasteboard as an RTFD attachment.
// NSRTFDPboardType must be declared on the pasteboard before calling this method.
- (void)_web_writeFileWrapperAsRTFDAttachment:(NSFileWrapper *)wrapper;

// Writes an image, URL and other optional types to the pasteboard.
- (void)_web_writeImage:(WebImageRenderer *)image 
                    URL:(NSURL *)URL 
                  title:(NSString *)title
                archive:(WebArchive *)archive
                  types:(NSArray *)types;

- (id)_web_declareAndWriteDragImage:(WebImageRenderer *)image 
                                URL:(NSURL *)URL 
                              title:(NSString *)title
                            archive:(WebArchive *)archive
                             source:(id)source;

@end
