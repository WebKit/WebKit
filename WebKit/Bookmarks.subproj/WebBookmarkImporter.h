//
//  WebBookmarkImporter.h
//  WebKit
//
//  Created by Ken Kocienda on Sun Nov 17 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class WebError;
@class WebBookmark;

#define InternetExplorerBookmarksPath \
    ([NSString stringWithFormat:@"%@/%@", NSHomeDirectory(), @"Library/Preferences/Explorer/Favorites.html"])

@interface WebBookmarkImporter : NSObject 
{
    WebBookmark *topBookmark;
    WebError *error;
}

-(id)initWithPath:(NSString *)path;
-(WebBookmark *)topBookmark;
-(WebError *)error;

@end
