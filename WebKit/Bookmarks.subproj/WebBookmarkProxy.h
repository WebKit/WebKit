//
//  WebBookmarkProxy.h
//  WebKit
//
//  Created by John Sullivan on Fri Oct 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBookmark.h>


@interface WebBookmarkProxy : WebBookmark {
    NSString *_title;
}

- (id)initWithTitle:(NSString *)title group:(WebBookmarkGroup *)group;

@end
