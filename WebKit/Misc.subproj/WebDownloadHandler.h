//
//  WebDownloadHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebKit/WebDataSource.h>

@interface WebDownloadHandler : NSObject {
    WebDataSource *dataSource;
    NSFileHandle *fileHandle;
}

- initWithDataSource:(WebDataSource *)dSource;
- (void)receivedData:(NSData *)data;
- (void)finishedLoading;
- (void)cancel;
@end
