/*
        NSCarbonWindowFrame.h
        Application Kit
        Copyright (c) 2000-2001, Apple Computer, Inc.
        All rights reserved.
        Original Author: Mark Piccirelli
        Responsibility: Mark Piccirelli

The class of objects that are meant to be used as _borderViews of NSCarbonWindow objects.  It's really only a subclass of NSView for the benefit of embedded NSViewCarbonControls.
*/


#import <AppKit/AppKit.h>


//@class NSCarbonWindow;
//@class NSCarbonWindowContentView;


@interface CarbonWindowFrame : NSView

// Instance variables.
{

    @private
    
    // Something we keep around just to return when it's asked for.
    unsigned int _styleMask;

}

@end // interface NSCarbonWindowFrame
