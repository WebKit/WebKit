//
//  IFWebCoreBridge.h
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreBridge.h>

@class IFWebDataSource;

@interface IFWebCoreBridge : WebCoreBridge
{
    IFWebDataSource *dataSource;
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource;

@end
