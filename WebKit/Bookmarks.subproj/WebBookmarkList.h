//
//  WebBookmarkList.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmark.h>

@interface WebBookmarkList : WebBookmark {
    NSString *_title;
    NSMutableArray *_list;
}

- (id)initWithTitle:(NSString *)title group:(WebBookmarkGroup *)group;

@end
