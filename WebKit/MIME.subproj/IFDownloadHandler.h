//
//  IFDownloadHandler.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebKit/IFMIMEHandler.h>
#import <WebFoundation/WebFoundation.h>

@interface IFDownloadHandler : NSObject {
@private
    id _downloadHandlerPrivate;
}

- (NSURL *) url;
- (IFMIMEHandler *) mimeHandler;
- (void) cancelDownload;
- (void) storeAtPath:(NSString *)path;
- (void) setOpenAfterDownload:(BOOL)open;
@end
