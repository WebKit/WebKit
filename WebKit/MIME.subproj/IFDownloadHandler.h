//
//  IFDownloadHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@class IFMIMEHandler;

@class IFDownloadHandlerPrivate;

@interface IFDownloadHandler : NSObject {
@private
    IFDownloadHandlerPrivate *_private;
}

- (NSURL *) url;
- (IFMIMEHandler *) mimeHandler;
- (NSString *) suggestedFilename;
- (void) cancelDownload;
- (void) storeAtPath:(NSString *)path;
- (void) openAfterDownload:(BOOL)open;
@end
