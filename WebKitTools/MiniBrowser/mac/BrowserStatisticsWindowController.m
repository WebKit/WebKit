//
//  BrowserStatisticsWindowController.m
//  MiniBrowser
//
//  Created by Sam Weinig on 4/21/10.
//  Copyright 2010 Apple Inc. All rights reserved.
//

#import "BrowserStatisticsWindowController.h"

#import <WebKit2/WKContextPrivate.h>

@implementation BrowserStatisticsWindowController

- (id)initWithThreadedWKContextRef:(WKContextRef)threadContext processWKContextRef:(WKContextRef)processContext
{
    if ((self = [super initWithWindowNibName:@"BrowserStatisticsWindow"])) {
        _threadContext = WKRetain(threadContext);
        _processContext = WKRetain(processContext);
    }

    return self;
}

- (void)dealloc
{
    WKRelease(_threadContext);
    _threadContext = 0;

    WKRelease(_processContext);
    _processContext = 0;
    
    [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    [self refreshStatistics:nil];
}

- (IBAction)refreshStatistics:(id)sender
{
    // FIXME: (Re-)implement.
}

@end
