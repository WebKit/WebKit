//
//  WKBackForwardList.h
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WKURIList.h"
#import "WKURIEntry.h"
#import <WCBackForwardList.h>

@interface WKBackForwardList : NSObject <WCBackForwardList> {
    WKURIList *uriList;
    int index;
    NSLock *mutex;
    int state;
}

-(id)init;

-(void)addEntry:(WKURIEntry *)entry;
-(WKURIEntry *)back;
-(WKURIEntry *)forward;

-(NSArray *)backList;
-(NSArray *)forwardList;

-(BOOL)canGoBack;
-(BOOL)canGoForward;

@end
