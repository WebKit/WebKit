//
//  IFWebCoreBridge.h
//  WebKit
//
//  Created by Darin Adler on Thu Jun 13 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreBridge.h>

#import <WebKit/IFWebDataSource.h>

@interface IFWebCoreBridge : WebCoreBridge <WebCoreBridge>
{
    IFWebDataSource *dataSource;
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource;
- (void)setDataSource: (IFWebDataSource *)ds;

@end

// Convenience interface for getting here from an IFWebDataSource.
// This returns nil if the representation is not an IFHTMLRepresentation.

@interface IFWebDataSource (IFWebCoreBridge)
- (IFWebCoreBridge *)_bridge;
@end
