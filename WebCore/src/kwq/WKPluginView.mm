//
//  WKPluginView.m
//  
//
//  Created by Chris Blumenberg on Thu Dec 13 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKPluginView.h"


@implementation WKPluginView

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    widget = w;
    isFlipped = YES;
    return self;
}

- (void)drawRect:(NSRect)rect {
    // Drawing code here.
}

@end
