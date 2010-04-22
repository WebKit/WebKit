//
//  BrowserStatisticsWindowController.h
//  MiniBrowser
//
//  Created by Sam Weinig on 4/21/10.
//  Copyright 2010 Apple Inc. All rights reserved.
//

@interface BrowserStatisticsWindowController : NSWindowController {
    IBOutlet NSMatrix *_basicStatsMatrix;

    WKContextRef _threadContext;
    WKContextRef _processContext;
}

- (id)initWithThreadedWKContextRef:(WKContextRef)threadContext processWKContextRef:(WKContextRef)processContext;

- (IBAction)refreshStatistics:(id)sender;

@end
