//
//  WebNSPasteboardExtras.h
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface NSPasteboard (WebExtras)

// Returns the array of drag types that _web_bestURL handles; note that the presence
// of one or more of these drag types on the pasteboard is not a guarantee that
// _web_bestURL will return a non-nil result.
+ (NSArray *)_web_dragTypesForURL;


// Finds the best URL from the data on the pasteboard, giving priority to http and https URLs
-(NSURL *)_web_bestURL;

@end
