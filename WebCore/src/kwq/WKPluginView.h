//
//  WKPluginView.h
//  
//
//  Created by Chris Blumenberg on Thu Dec 13 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>
#include <qwidget.h>

@interface WKPluginView : NSQuickDrawView {
    QWidget *widget;
    bool isFlipped;
}

- initWithFrame: (NSRect) r widget: (QWidget *)w;

@end
