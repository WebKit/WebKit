//
//  WebBridge.h
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreBridge.h>

#import <WebKit/WebDataSource.h>

@interface WebBridge : WebCoreBridge <WebCoreBridge>
{
    WebDataSource *dataSource;
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
- (void)setDataSource:(WebDataSource *)ds;

@end

