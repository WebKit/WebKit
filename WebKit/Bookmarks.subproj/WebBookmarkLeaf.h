//
//  IFBookmarkLeaf.h
//  WebKit
//
//  Created by John Sullivan on Tue Apr 30 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFBookmark.h>

@class IFURIEntry;

@interface IFBookmarkLeaf : IFBookmark {
    IFURIEntry *_entry;
    NSString *_URLString;
}

- (id)initWithURLString:(NSString *)URLString
                  title:(NSString *)title
                  image:(NSImage *)image
                  group:(IFBookmarkGroup *)group;

@end
