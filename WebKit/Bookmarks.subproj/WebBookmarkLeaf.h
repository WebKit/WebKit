//
//  WebBookmarkLeaf.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmark.h>

@class WebHistoryItem;

@interface WebBookmarkLeaf : WebBookmark {
    WebHistoryItem *_entry;
    NSString *_URLString;
}

- (id)initWithURLString:(NSString *)URLString
                  title:(NSString *)title
                  group:(WebBookmarkGroup *)group;

@end
