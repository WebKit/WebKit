//
//  IFDownloadHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebKit/IFWebDataSource.h>

@interface IFDownloadHandler : NSObject {
    IFWebDataSource *dataSource;
    NSFileHandle *fileHandle;
}

- initWithDataSource:(IFWebDataSource *)dSource;
- (void)receivedData:(NSData *)data;
- (void)finishedLoading;
- (void)cancel;
@end
