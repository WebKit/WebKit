//
//  NSCarbonWindowAdapter.h
//  Synergy
//
//  Created by Ed Voas on Fri Jan 17 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <HIToolbox/CarbonEvents.h>
#import <HIToolbox/MacWindows.h>

@interface CarbonWindowAdapter : NSWindow 
{
@private

    // The Carbon window that's being encapsulated, and whether or not this object owns (has responsibility for disposing) it.
    WindowRef _windowRef;
    BOOL _windowRefIsOwned;
    BOOL _carbon;
	
    // The UPP for the event handler that we use to deal with various Carbon events, and the event handler itself.
    EventHandlerUPP _handleEventUPP;
    EventHandlerRef _eventHandler;
    
    // Yes if this object should let Carbon handle kEventWindowActivated and kEventWindowDeactivated events.  No otherwise.
    BOOL _passingCarbonWindowActivationEvents;
}

// Initializers.
- (id)initWithCarbonWindowRef:(WindowRef)inWindowRef takingOwnership:(BOOL)inWindowRefIsOwned;
- (id)initWithCarbonWindowRef:(WindowRef)inWindowRef takingOwnership:(BOOL)inWindowRefIsOwned disableOrdering:(BOOL)inDisableOrdering carbon:(BOOL)inCarbon;

// Accessors.
- (WindowRef)windowRef;

// Update this window's frame and content rectangles to match the Carbon window's structure and content bounds rectangles.  Return yes if the update was really necessary, no otherwise.
- (BOOL)reconcileToCarbonWindowBounds;

// Handle an event just like an NSWindow would.
- (void)sendSuperEvent:(NSEvent *)inEvent;

- (void)relinquishFocus;

@end
