//
//  WebSearchableTextView.h
//  WebKit
//
//  Created by John Sullivan on Fri Aug 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol WebDocumentSearching;

@interface WebSearchableTextView : NSTextView <WebDocumentSearching>
@end
