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
}

- initWithDataSource:(IFWebDataSource *)dSource;
- (void)downloadCompletedWithData:(NSData *)data;

+ (void) launchURL:(NSURL *) url;
@end
