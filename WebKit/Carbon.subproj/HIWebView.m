/*
 *  HIWebFrameView.c
 *  Synergy
 *
 *  Created by Ed Voas on Tue Feb 11 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "HIWebViewPriv.h"
//#include "HIWebController.h"

#include "CarbonWindowAdapter.h"

#include <AppKit/NSGraphicsContextPrivate.h>
#include <CoreGraphics/CGSEvent.h>
#include <WebKit/WebKit.h>
#include "HIViewAdapter.h"

extern Boolean GetEventPlatformEventRecord( EventRef inEvent, void * eventRec );

struct HIWebFrameView
{
    HIViewRef							fViewRef;

    WebFrameView*						fWebFrameView;
    NSView*								fFirstResponder;
    CarbonWindowAdapter	*				fKitWindow;
    bool								fIsComposited;
    CFRunLoopObserverRef				fUpdateObserver;
};
typedef struct HIWebFrameView HIWebFrameView;

@interface NSGraphicsContext (Secret)
- (void)setCGContext:(CGContextRef)cgContext;
@end

@interface NSWindow(Secret)
- (NSGraphicsContext *)_threadContext;
- (NSView *)_borderView;
- (void)_removeWindowRef;
@end

@interface NSEvent( Secret )
- (NSEvent *)_initWithCGSEvent:(CGSEventRecord)cgsEvent eventRef:(void *)eventRef;
@end

@interface NSView( Secret )
- (void) _setHIView:(HIViewRef)view;
- (void) _clearDirtyRectsForTree;
- (BOOL) _allowsContextMenus;
@end

@interface NSMenu( Secret )
- (void)_popUpMenuWithEvent:(NSEvent*)event forView:(NSView*)view;
@end

@interface MenuItemProxy : NSObject <NSValidatedUserInterfaceItem> {
	int	_tag;
	SEL _action;
}

- (id)initWithAction:(SEL)action;
- (SEL)action;
- (int)tag;

@end

@implementation MenuItemProxy

- (id)initWithAction:(SEL)action {
	[super init];
    if (self == nil) return nil;
	
	_action = action;
	
	return self;
}

- (SEL)action {
	return _action;
}

- (int)tag {
	return 0;
}
@end


@implementation NSWindowGraphicsContext (NSHLTBAdditions)
- (void)setCGContext:(CGContextRef)cgContext {
    CGContextRetain(cgContext);
    if (_cgsContext) {
        CGContextRelease(_cgsContext);
    }
    _cgsContext = cgContext;
}
@end

static const OSType NSAppKitPropertyCreator = 'akit';
static const OSType NSViewCarbonControlViewPropertyTag = 'view';
static const OSType NSViewCarbonControlAutodisplayPropertyTag = 'autd';
static const OSType NSViewCarbonControlFirstResponderViewPropertyTag = 'frvw';
static const OSType NSCarbonWindowPropertyTag = 'win ';

static SEL _NSSelectorForHICommand( const HICommand* hiCommand );

static const EventTypeSpec kEvents[] = { 
	{ kEventClassHIObject, kEventHIObjectConstruct },
	{ kEventClassHIObject, kEventHIObjectDestruct },
	
	{ kEventClassMouse, kEventMouseUp },
	{ kEventClassMouse, kEventMouseMoved },
	{ kEventClassMouse, kEventMouseDragged },
	{ kEventClassMouse, kEventMouseWheelMoved },

	{ kEventClassKeyboard, kEventRawKeyDown },
	{ kEventClassKeyboard, kEventRawKeyRepeat },

	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassCommand, kEventCommandUpdateStatus },

	{ kEventClassControl, kEventControlInitialize },
	{ kEventClassControl, kEventControlDraw },
	{ kEventClassControl, kEventControlHitTest },
	{ kEventClassControl, kEventControlGetPartRegion },
	{ kEventClassControl, kEventControlGetData },
	{ kEventClassControl, kEventControlBoundsChanged },
	{ kEventClassControl, kEventControlActivate },
	{ kEventClassControl, kEventControlDeactivate },
	{ kEventClassControl, kEventControlOwningWindowChanged },
	{ kEventClassControl, kEventControlClick },
	{ kEventClassControl, kEventControlContextualMenuClick },
	{ kEventClassControl, kEventControlSetFocusPart }
};

#define kHIViewBaseClassID		CFSTR( "com.apple.hiview" )
#define kHIWebFrameViewClassID		CFSTR( "com.apple.HIWebFrameView" )

static HIWebFrameView*		HIWebFrameViewConstructor( HIViewRef inView );
static void				HIWebFrameViewDestructor( HIWebFrameView* view );
static void				HIWebFrameViewRegisterClass( void );

static OSStatus			HIWebFrameViewEventHandler(
								EventHandlerCallRef	inCallRef,
								EventRef			inEvent,
								void *				inUserData );

static UInt32			GetBehaviors();
static ControlKind		GetKind();
static void				Draw( HIWebFrameView* inView, RgnHandle limitRgn, CGContextRef inContext );
static ControlPartCode	HitTest( HIWebFrameView* view, const HIPoint* where );
static OSStatus			GetRegion( HIWebFrameView* view, ControlPartCode inPart, RgnHandle outRgn );
static void				BoundsChanged(
								HIWebFrameView*			inView,
								UInt32				inOptions,
								const HIRect*		inOriginalBounds,
								const HIRect*		inCurrentBounds );
static void				OwningWindowChanged(
								HIWebFrameView*			view,
								WindowRef			oldWindow,
								WindowRef			newWindow );
static void				ActiveStateChanged( HIWebFrameView* view );

static OSStatus			Click( HIWebFrameView* inView, EventRef inEvent );
static OSStatus			ContextMenuClick( HIWebFrameView* inView, EventRef inEvent );
static OSStatus			MouseUp( HIWebFrameView* inView, EventRef inEvent );
static OSStatus			MouseMoved( HIWebFrameView* inView, EventRef inEvent );
static OSStatus			MouseDragged( HIWebFrameView* inView, EventRef inEvent );
static OSStatus			MouseWheelMoved( HIWebFrameView* inView, EventRef inEvent );

static OSStatus			ProcessCommand( HIWebFrameView* inView, const HICommand* inCommand );
static OSStatus			UpdateCommandStatus( HIWebFrameView* inView, const HICommand* inCommand );

static OSStatus			SetFocusPart(
								HIWebFrameView*				view,
								ControlPartCode 		desiredFocus,
								RgnHandle 				invalidRgn,
								Boolean 				focusEverything,
								ControlPartCode* 		actualFocus );
static NSView*			AdvanceFocus( HIWebFrameView* view, bool forward );
static void				RelinquishFocus( HIWebFrameView* view, bool inAutodisplay );

static WindowRef		GetWindowRef( HIWebFrameView* inView );
static void				SyncFrame( HIWebFrameView* inView );

static OSStatus			WindowCloseHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData );

static void				StartUpdateObserver( HIWebFrameView* view );
static void				StopUpdateObserver( HIWebFrameView* view );
static void 			UpdateObserver( CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info );

static inline void HIRectToQDRect( const HIRect* inRect, Rect* outRect )
{
    outRect->top = CGRectGetMinY( *inRect );
    outRect->left = CGRectGetMinX( *inRect );
    outRect->bottom = CGRectGetMaxY( *inRect );
    outRect->right = CGRectGetMaxX( *inRect );
}

//----------------------------------------------------------------------------------
// HIWebFrameViewCreate
//----------------------------------------------------------------------------------
//
OSStatus
HIWebFrameViewCreate( HIViewRef* outControl )
{
	OSStatus			err;
    
	HIWebFrameViewRegisterClass();

	err = HIObjectCreate( kHIWebFrameViewClassID, NULL, (HIObjectRef*)outControl );

	return err;
}

//----------------------------------------------------------------------------------
// HIWebFrameViewGetNSView
//----------------------------------------------------------------------------------
//
NSView*
HIWebFrameViewGetNSView( HIViewRef inView )
{
	HIWebFrameView* 	view = (HIWebFrameView*)HIObjectDynamicCast( (HIObjectRef)inView, kHIWebFrameViewClassID );
	NSView*				result = NULL;
	
	if ( view )
		result = view->fWebFrameView;
	
	return result;
}

//----------------------------------------------------------------------------------
// HIWebFrameViewGetController
//----------------------------------------------------------------------------------
//
WebView*
HIWebFrameViewGetController( HIViewRef inControl )
{
	HIWebFrameView* 			view = (HIWebFrameView*)HIObjectDynamicCast( (HIObjectRef)inControl, kHIWebFrameViewClassID );
	WebView*		result = NULL;
	
	if ( view )
		result = [[view->fWebFrameView webFrame] webView];
	
	return result;
}

extern WebView*
WebControllerCreateWithHIView( HIViewRef inView, CFStringRef inName )
{
#ifdef XXXX
	NSView*			view = HIWebFrameViewGetNSView( inView );
	WebView*	result = NULL;
	
	if ( view )
		result = [[WebView alloc] initWithView: (WebFrameView*)view frameName:nil groupName:(NSString*)inName];

	return result;
#else
        return NULL;
#endif
}

//----------------------------------------------------------------------------------
// HIWebFrameViewConstructor
//----------------------------------------------------------------------------------
//
static HIWebFrameView*
HIWebFrameViewConstructor( HIViewRef inView )
{
	HIWebFrameView*		view = (HIWebFrameView*)malloc( sizeof( HIWebFrameView ) );
	
	if ( view )
	{
		NSRect		frame = { { 0, 0 }, { 400, 400  } };
	
		view->fViewRef = inView;

		view->fWebFrameView = [[WebFrameView alloc] initWithFrame: frame];
		[HIViewAdapter bindHIViewToNSView:inView nsView:view->fWebFrameView];
		
		view->fFirstResponder = NULL;
		view->fKitWindow = NULL;
        view->fIsComposited = false;
        view->fUpdateObserver = NULL;
	}
	
	return view;
}

//----------------------------------------------------------------------------------
// HIWebFrameViewDestructor
//----------------------------------------------------------------------------------
//
static void
HIWebFrameViewDestructor( HIWebFrameView* inView )
{
	[HIViewAdapter unbindNSView:inView->fWebFrameView];
	[inView->fWebFrameView release];
	
	free( inView );
}

//----------------------------------------------------------------------------------
// HIWebFrameViewRegisterClass
//----------------------------------------------------------------------------------
//
static void
HIWebFrameViewRegisterClass()
{
	static bool sRegistered;
	
	if ( !sRegistered )
	{
		HIObjectRegisterSubclass( kHIWebFrameViewClassID, kHIViewBaseClassID, 0, HIWebFrameViewEventHandler,
			GetEventTypeCount( kEvents ), kEvents, 0, NULL );
		sRegistered = true;
	}
}

//----------------------------------------------------------------------------------
// GetBehaviors
//----------------------------------------------------------------------------------
//
static UInt32
GetBehaviors()
{
	return kControlSupportsDataAccess | kControlSupportsGetRegion | kControlGetsFocusOnClick;
}

//----------------------------------------------------------------------------------
// Draw
//----------------------------------------------------------------------------------
//
static void
Draw( HIWebFrameView* inView, RgnHandle limitRgn, CGContextRef inContext )
{
	HIRect				bounds;
	CGContextRef		temp;
	Rect				drawRect;
	HIRect				hiRect;
	bool				createdContext = false;
    GrafPtr				port;

    if ( !inView->fIsComposited )
    {
		Rect	portRect;

        GetPort( &port );
		GetPortBounds( port, &portRect );
        CreateCGContextForPort( port, &inContext );
        SyncCGContextOriginWithPort( inContext, port );
 		CGContextTranslateCTM( inContext, 0, (portRect.bottom - portRect.top) );
		CGContextScaleCTM( inContext, 1, -1 );
        createdContext = true;
    }
    
	HIViewGetBounds( inView->fViewRef, &bounds );

	temp = (CGContextRef)[[inView->fKitWindow _threadContext] graphicsPort];
	CGContextRetain( temp );
	[[inView->fKitWindow _threadContext] setCGContext: inContext];
	[NSGraphicsContext setCurrentContext:[inView->fKitWindow _threadContext] ];

	GetRegionBounds( limitRgn, &drawRect );

    if ( !inView->fIsComposited )
        OffsetRect( &drawRect, -bounds.origin.x, -bounds.origin.y );
    
	hiRect.origin.x = drawRect.left;
	hiRect.origin.y = bounds.size.height - drawRect.bottom; // flip y
	hiRect.size.width = drawRect.right - drawRect.left;
	hiRect.size.height = drawRect.bottom - drawRect.top;

//    printf( "Drawing: drawRect is (%g %g) (%g %g)\n", hiRect.origin.x, hiRect.origin.y,
//            hiRect.size.width, hiRect.size.height );

    if ( inView->fIsComposited )
        [inView->fWebFrameView displayIfNeededInRect: *(NSRect*)&hiRect];
    else
        [inView->fWebFrameView displayRect:*(NSRect*)&hiRect];

	[[inView->fKitWindow _threadContext] setCGContext: temp];

	CGContextRelease( temp );
    
    if ( createdContext )
    {
        CGContextSynchronize( inContext );
        CGContextRelease( inContext );
    }
}

//----------------------------------------------------------------------------------
// HitTest
//----------------------------------------------------------------------------------
//
static ControlPartCode
HitTest( HIWebFrameView* view, const HIPoint* where )
{
	HIRect		bounds;
	
	HIViewGetBounds( view->fViewRef, &bounds );

	if ( CGRectContainsPoint( bounds, *where ) )
		return 1;
	else
		return kControlNoPart;
}

//----------------------------------------------------------------------------------
// GetRegion
//----------------------------------------------------------------------------------
//
static OSStatus
GetRegion(
	HIWebFrameView*			inView,
	ControlPartCode		inPart,
	RgnHandle			outRgn )
{
	OSStatus	 err = eventNotHandledErr;
	
	if ( inPart == -3 ) // kControlOpaqueMetaPart:
	{
		if ( [inView->fWebFrameView isOpaque] )
		{
			HIRect	bounds;
			Rect	temp;
			
			HIViewGetBounds( inView->fViewRef, &bounds );

			temp.top = (SInt16)bounds.origin.y;
			temp.left = (SInt16)bounds.origin.x;
			temp.bottom = CGRectGetMaxY( bounds );
			temp.right = CGRectGetMaxX( bounds );

			RectRgn( outRgn, &temp );
			err = noErr;
		}
	}
	
	return err;
}

//----------------------------------------------------------------------------------
// GetRegion
//----------------------------------------------------------------------------------
//
static WindowRef
GetWindowRef( HIWebFrameView* inView )
{
	return GetControlOwner( inView->fViewRef );
}

//----------------------------------------------------------------------------------
// Click
//----------------------------------------------------------------------------------
//
static OSStatus
Click( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
	ControlRef				focus;
//	NSView*					targ;
	EventRef				newEvent;
	Point					where;
	OSStatus				err;
	UInt32					modifiers;
	Rect					windRect;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );

	// We need to make the event be a kEventMouseDown event, or the webkit might trip up when
	// we click on a Netscape plugin. It calls ConvertEventRefToEventRecord, assuming
	// that mouseDown was passed an event with a real mouse down eventRef. We just need a
	// minimal one here.

	err = CreateEvent( NULL, kEventClassMouse, kEventMouseDown, GetEventTime( inEvent ), 0, &newEvent );
	require_noerr( err, CantAllocNewEvent );

	GetEventParameter( inEvent, kEventParamWindowMouseLocation, typeQDPoint, NULL,
			sizeof( Point ), NULL, &where );
	GetWindowBounds( GetWindowRef( inView ), kWindowStructureRgn, &windRect );
	where.h += windRect.left;
	where.v += windRect.top;
	
	GetEventParameter( inEvent, kEventParamKeyModifiers, typeUInt32, NULL,
			sizeof( UInt32 ), NULL, &modifiers );
	SetEventParameter( newEvent, kEventParamMouseLocation, typeQDPoint, sizeof( Point ), &where );
	SetEventParameter( newEvent, kEventParamKeyModifiers, typeUInt32, sizeof( UInt32 ), &modifiers );
	
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)newEvent];

	// Grab the keyboard focus
	// еее FIX: Need to switch to a real part code, not focusnextpart. Have to handle
	//			subviews properly as well.

	GetKeyboardFocus( GetWindowRef( inView ), &focus );
	if ( focus != inView->fViewRef )
		SetKeyboardFocus( GetWindowRef( inView ), inView->fViewRef, kControlFocusNextPart );

//	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

    if ( !inView->fIsComposited )
        StartUpdateObserver( inView );
        
//	[targ mouseDown:kitEvent];
    [inView->fKitWindow sendEvent:kitEvent];

    if ( !inView->fIsComposited )
        StopUpdateObserver( inView );

	[kitEvent release];

CantAllocNewEvent:	
	return noErr;
}

//----------------------------------------------------------------------------------
// MouseUp
//----------------------------------------------------------------------------------
//
static OSStatus
MouseUp( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
//	NSView*					targ;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );
	RetainEvent( inEvent );
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

//	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

//	[targ mouseUp:kitEvent];

    [inView->fKitWindow sendEvent:kitEvent];
	
    [kitEvent release];
    
	return noErr;
}

//----------------------------------------------------------------------------------
// MouseMoved
//----------------------------------------------------------------------------------
//
static OSStatus
MouseMoved( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
//	NSView*					targ;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );
	RetainEvent( inEvent );
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

//	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

//	[targ mouseMoved:kitEvent];
    [inView->fKitWindow sendEvent:kitEvent];

	[kitEvent release];
	
	return noErr;
}

//----------------------------------------------------------------------------------
// MouseDragged
//----------------------------------------------------------------------------------
//
static OSStatus
MouseDragged( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
//	NSView*					targ;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );
	RetainEvent( inEvent );
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

//	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

//	[targ mouseDragged:kitEvent];
    [inView->fKitWindow sendEvent:kitEvent];

	[kitEvent release];
	
	return noErr;
}

//----------------------------------------------------------------------------------
// MouseDragged
//----------------------------------------------------------------------------------
//
static OSStatus
MouseWheelMoved( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
//	NSView*					targ;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );
	RetainEvent( inEvent );
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

//	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

//	[targ scrollWheel:kitEvent];
    [inView->fKitWindow sendEvent:kitEvent];

	[kitEvent release];
	
	return noErr;
}

//----------------------------------------------------------------------------------
// ContextMenuClick
//----------------------------------------------------------------------------------
//
static OSStatus
ContextMenuClick( HIWebFrameView* inView, EventRef inEvent )
{
	CGSEventRecord			eventRec;
	NSEvent*				kitEvent;
	OSStatus				result = eventNotHandledErr;
	NSView*					targ;
	
	GetEventPlatformEventRecord( inEvent, &eventRec );
	RetainEvent( inEvent );
	kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

	targ = [[inView->fKitWindow _borderView] hitTest:[kitEvent locationInWindow]];

    if ( [targ _allowsContextMenus] )
	{
        NSMenu * contextMenu = [targ menuForEvent:kitEvent];

        if ( contextMenu )
		{
            [contextMenu _popUpMenuWithEvent:kitEvent forView:targ];
			result = noErr;
        }
    }

	[kitEvent release];
	
	return result;
}

//----------------------------------------------------------------------------------
// GetKind
//----------------------------------------------------------------------------------
//
static ControlKind
GetKind()
{
	const ControlKind kMyKind = { 'appl', 'wbvw' };
	
	return kMyKind;
}

//----------------------------------------------------------------------------------
// BoundsChanged
//----------------------------------------------------------------------------------
//
static void
BoundsChanged(
	HIWebFrameView*			inView,
	UInt32				inOptions,
	const HIRect*		inOriginalBounds,
	const HIRect*		inCurrentBounds )
{
	if ( inView->fWebFrameView )
	{
		SyncFrame( inView );
	}
}

//----------------------------------------------------------------------------------
// OwningWindowChanged
//----------------------------------------------------------------------------------
//
static void
OwningWindowChanged(
	HIWebFrameView*			view,
	WindowRef			oldWindow,
	WindowRef			newWindow )
{
	if ( newWindow )
	{
        WindowAttributes	attrs;
        
    	OSStatus err = GetWindowProperty(newWindow, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), NULL, &view->fKitWindow);
		if ( err != noErr )
		{
			const EventTypeSpec kWindowEvents[] = { { kEventClassWindow, kEventWindowClosed } };

			view->fKitWindow = [[CarbonWindowAdapter alloc] initWithCarbonWindowRef: newWindow takingOwnership: NO disableOrdering:NO carbon:YES];
    		SetWindowProperty(newWindow, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), &view->fKitWindow);
		
			InstallWindowEventHandler( newWindow, WindowCloseHandler, GetEventTypeCount( kWindowEvents ), kWindowEvents, newWindow, NULL );
		}
		
		[[view->fKitWindow contentView] addSubview:view->fWebFrameView];

        GetWindowAttributes( newWindow, &attrs );
        view->fIsComposited = ( ( attrs & kWindowCompositingAttribute ) != 0 );

		SyncFrame( view );        
	}
}

//----------------------------------------------------------------------------------
// WindowCloseHandler
//----------------------------------------------------------------------------------
//
static OSStatus
WindowCloseHandler( EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData )
{
	WindowRef	window = (WindowRef)inUserData;
	OSStatus	err;
	NSWindow*	kitWindow;

	err = GetWindowProperty( window, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), NULL, &kitWindow);
	if ( err == noErr )
	{
		[kitWindow _removeWindowRef];
		[kitWindow close];
	}
	
	return noErr;
}


//----------------------------------------------------------------------------------
// SyncFrame
//----------------------------------------------------------------------------------
//
static void
SyncFrame( HIWebFrameView* inView )
{
	HIViewRef	parent = HIViewGetSuperview( inView->fViewRef );
	
	if ( parent )
	{
        if ( inView->fIsComposited )
        {
            HIRect		frame;
            HIRect		parentBounds;
            NSPoint		origin;

            HIViewGetFrame( inView->fViewRef, &frame );
            HIViewGetBounds( parent, &parentBounds );
            
            origin.x = frame.origin.x;
            origin.y = parentBounds.size.height - CGRectGetMaxY( frame );
    printf( "syncing to (%g %g) (%g %g)\n", origin.x, origin.y,
            frame.size.width, frame.size.height );
            [inView->fWebFrameView setFrameOrigin: origin];
            [inView->fWebFrameView setFrameSize: *(NSSize*)&frame.size];
        }
        else
        {
            GrafPtr			port = GetWindowPort( GetControlOwner( inView->fViewRef ) );
            PixMapHandle	portPix = GetPortPixMap( port );
            Rect			bounds;
            HIRect			rootFrame;
            HIRect			frame;

            GetControlBounds( inView->fViewRef, &bounds );
            OffsetRect( &bounds, -(**portPix).bounds.left, -(**portPix).bounds.top );

//            printf( "control lives at %d %d %d %d in window-coords\n", bounds.top, bounds.left,
//                bounds.bottom, bounds.right );
  
            HIViewGetFrame( HIViewGetRoot( GetControlOwner( inView->fViewRef ) ), &rootFrame );

            frame.origin.x = bounds.left;
            frame.origin.y = rootFrame.size.height - bounds.bottom;
            frame.size.width = bounds.right - bounds.left;
            frame.size.height = bounds.bottom - bounds.top;

//            printf( "   before frame convert (%g %g) (%g %g)\n", frame.origin.x, frame.origin.y,
//                frame.size.width, frame.size.height );
            
            [inView->fWebFrameView convertRect:*(NSRect*)&frame fromView:nil];

//            printf( "   moving web view to (%g %g) (%g %g)\n", frame.origin.x, frame.origin.y,
//                frame.size.width, frame.size.height );

            [inView->fWebFrameView setFrameOrigin: *(NSPoint*)&frame.origin];
            [inView->fWebFrameView setFrameSize: *(NSSize*)&frame.size];
        }
	}
}

//----------------------------------------------------------------------------------
// SetFocusPart
//----------------------------------------------------------------------------------
//
static OSStatus
SetFocusPart(
	HIWebFrameView*				view,
	ControlPartCode 		desiredFocus,
	RgnHandle 				invalidRgn,
	Boolean 				focusEverything,
	ControlPartCode* 		actualFocus )
{
    NSView *	freshlyMadeFirstResponderView;
    SInt32 		partCodeToReturn;

    // Do what Carbon is telling us to do.
    if ( desiredFocus == kControlFocusNoPart )
	{
        // Relinquish the keyboard focus.
        RelinquishFocus( view, true ); //(autodisplay ? YES : NO));
        freshlyMadeFirstResponderView = nil;
        partCodeToReturn = kControlFocusNoPart;
		//NSLog(@"Relinquished the key focus because we have no choice.");
    }
	else if ( desiredFocus == kControlFocusNextPart || desiredFocus == kControlFocusPrevPart )
	{
        BOOL goForward = (desiredFocus == kControlFocusNextPart );

        // Advance the keyboard focus, maybe right off of this view.  Maybe a subview of this one already has the keyboard focus, maybe not.
        freshlyMadeFirstResponderView = AdvanceFocus( view, goForward );
        partCodeToReturn = freshlyMadeFirstResponderView ? desiredFocus : kControlFocusNoPart;
        //NSLog(freshlyMadeFirstResponderView ? @"Advanced the key focus." : @"Relinquished the key focus.");
    }
	else
	{
		// What's this?
		check(false);
		freshlyMadeFirstResponderView = nil;
		partCodeToReturn = kControlFocusNoPart;
    }

	view->fFirstResponder = freshlyMadeFirstResponderView;

	*actualFocus = partCodeToReturn;

	// Done.
	return noErr;
}

//----------------------------------------------------------------------------------
// AdvanceFocus
//----------------------------------------------------------------------------------
//
static NSView*
AdvanceFocus( HIWebFrameView* view, bool forward )
{
    NSResponder*		oldFirstResponder;
    NSView*				currentKeyView;
    NSView*				viewWeMadeFirstResponder;
    
    //	Focus on some part (subview) of this control (view).  Maybe
	//	a subview of this one already has the keyboard focus, maybe not.
	
	oldFirstResponder = [view->fKitWindow firstResponder];

	// If we tab out of our NSView, it will no longer be the responder
	// when we get here. We'll try this trick for now. We might need to
	// tag the view appropriately.

	if ( view->fFirstResponder && ( (NSResponder*)view->fFirstResponder != oldFirstResponder ) )
	{
		return NULL;
	}
	
	if ( [oldFirstResponder isKindOfClass:[NSView class]] )
	{
		NSView*		tentativeNewKeyView;

        // Some view in this window already has the keyboard focus.  It better at least be a subview of this one.
        NSView*	oldFirstResponderView = (NSView *)oldFirstResponder;
        check( [oldFirstResponderView isDescendantOf:view->fWebFrameView] );

		if ( oldFirstResponderView != view->fFirstResponder
			&& ![oldFirstResponderView isDescendantOf:view->fFirstResponder] )
		{
            // Despite our efforts to record what view we made the first responder
			// (for use in the next paragraph) we couldn't keep up because the user
			// has clicked in a text field to make it the key focus, instead of using
			// the tab key.  Find a control on which it's reasonable to invoke
			// -[NSView nextValidKeyView], taking into account the fact that
			// NSTextFields always pass on first-respondership to a temporarily-
			// contained NSTextView.

			NSView *viewBeingTested;
			currentKeyView = oldFirstResponderView;
			viewBeingTested = currentKeyView;
			while ( viewBeingTested != view->fWebFrameView )
			{
				if ( [viewBeingTested isKindOfClass:[NSTextField class]] )
				{
					currentKeyView = viewBeingTested;
					break;
				}
				else
				{
					viewBeingTested = [viewBeingTested superview];
				}
			}
		}
		else 
		{
			// We recorded which view we made into the first responder the
			// last time the user hit the tab key, and nothing has invalidated
			// our recorded value since.
			
			currentKeyView = view->fFirstResponder;
		}

        // Try to move on to the next or previous key view.  We use the laboriously
		// recorded/figured currentKeyView instead of just oldFirstResponder as the
		// jumping-off-point when searching for the next valid key view.  This is so
		// we don't get fooled if we recently made some view the first responder, but
		// it passed on first-responder-ness to some temporary subview.
		
        // You can't put normal views in a window with Carbon-control-wrapped views.
		// Stuff like this would break.  M.P. Notice - 12/2/00

        tentativeNewKeyView = forward ? [currentKeyView nextValidKeyView] : [currentKeyView previousValidKeyView];
        if ( tentativeNewKeyView && [tentativeNewKeyView isDescendantOf:view->fWebFrameView] )
		{
            // The user has tabbed to another subview of this control view.  Change the keyboard focus.
            //NSLog(@"Tabbed to the next or previous key view.");

            [view->fKitWindow makeFirstResponder:tentativeNewKeyView];
            viewWeMadeFirstResponder = tentativeNewKeyView;
        }
		else
		{
            // The user has tabbed past the subviews of this control view.  The window is the first responder now.
            //NSLog(@"Tabbed past the first or last key view.");
            [view->fKitWindow makeFirstResponder:view->fKitWindow];
            viewWeMadeFirstResponder = nil;
        }
    }
	else
	{
        // No view in this window has the keyboard focus.  This view should
		// try to select one of its key subviews.  We're not interested in
		// the subviews of sibling views here.

		//NSLog(@"No keyboard focus in window. Attempting to set...");

		NSView *tentativeNewKeyView;
		check(oldFirstResponder==fKitWindow);
		if ( [view->fWebFrameView acceptsFirstResponder] )
			tentativeNewKeyView = view->fWebFrameView;
		else
			tentativeNewKeyView = [view->fWebFrameView nextValidKeyView];
        if ( tentativeNewKeyView && [tentativeNewKeyView isDescendantOf:view->fWebFrameView] )
		{
            // This control view has at least one subview that can take the keyboard focus.
            if ( !forward )
			{
                // The user has tabbed into this control view backwards.  Find
				// and select the last subview of this one that can take the
				// keyboard focus.  Watch out for loops of valid key views.

                NSView *firstTentativeNewKeyView = tentativeNewKeyView;
                NSView *nextTentativeNewKeyView = [tentativeNewKeyView nextValidKeyView];
                while ( nextTentativeNewKeyView 
						&& [nextTentativeNewKeyView isDescendantOf:view->fWebFrameView] 
						&& nextTentativeNewKeyView!=firstTentativeNewKeyView)
				{
                    tentativeNewKeyView = nextTentativeNewKeyView;
                    nextTentativeNewKeyView = [tentativeNewKeyView nextValidKeyView];
                }

            }

            // Set the keyboard focus.
            //NSLog(@"Tabbed into the first or last key view.");
            [view->fKitWindow makeFirstResponder:tentativeNewKeyView];
            viewWeMadeFirstResponder = tentativeNewKeyView;
        }
		else
		{
            // This control view has no subviews that can take the keyboard focus.
            //NSLog(@"Can't tab into this view.");
            viewWeMadeFirstResponder = nil;
        }
    }

    // Done.
    return viewWeMadeFirstResponder;
}


//----------------------------------------------------------------------------------
// RelinquishFocus
//----------------------------------------------------------------------------------
//
static void
RelinquishFocus( HIWebFrameView* view, bool inAutodisplay )
{
    NSResponder*  firstResponder;

    // Apparently Carbon thinks that some subview of this control view has the keyboard focus,
	// or we wouldn't be being asked to relinquish focus.

	firstResponder = [view->fKitWindow firstResponder];
	if ( [firstResponder isKindOfClass:[NSView class]] )
	{
		// Some subview of this control view really is the first responder right now.
		check( [(NSView *)firstResponder isDescendantOf:view->fWebFrameView] );

		// Make the window the first responder, so that no view is the key view.
        [view->fKitWindow makeFirstResponder:view->fKitWindow];

		// 	If this control is not allowed to do autodisplay, don't let
		//	it autodisplay any just-changed focus rings or text on the
		//	next go around the event loop. I'm probably clearing more
		//	dirty rects than I have to, but it doesn't seem to hurt in
		//	the print panel accessory view case, and I don't have time
		//	to figure out exactly what -[NSCell _setKeyboardFocusRingNeedsDisplay]
		//	is doing when invoked indirectly from -makeFirstResponder up above.  M.P. Notice - 12/4/00

		if ( !inAutodisplay )
			[[view->fWebFrameView opaqueAncestor] _clearDirtyRectsForTree];
    }
	else
	{
		//  The Cocoa first responder does not correspond to the Carbon
		//	control that has the keyboard focus.  This can happen when
		//	you've closed a dialog by hitting return in an NSTextView
		//	that's a subview of this one; Cocoa closed the window, and
		//	now Carbon is telling this control to relinquish the focus
		//	as it's being disposed.  There's nothing to do.

		check(firstResponder==window);
	}
}

//----------------------------------------------------------------------------------
// ActiveStateChanged
//----------------------------------------------------------------------------------
//
static void
ActiveStateChanged( HIWebFrameView* view )
{
	if ( [view->fWebFrameView respondsToSelector:@selector(setEnabled)] )
	{
		[(NSControl*)view->fWebFrameView setEnabled: IsControlEnabled( view->fViewRef )];
		HIViewSetNeedsDisplay( view->fViewRef, true );
	}
}


//----------------------------------------------------------------------------------
// ProcessCommand
//----------------------------------------------------------------------------------
//
static OSStatus
ProcessCommand( HIWebFrameView* inView, const HICommand* inCommand )
{
	OSStatus		result = eventNotHandledErr;
	NSResponder*	resp;
	
	resp = [inView->fKitWindow firstResponder];

	if ( [resp isKindOfClass:[NSView class]] )
	{
		NSView*	respView = (NSView*)resp;

		if ( respView == inView->fWebFrameView
			|| [respView isDescendantOf: inView->fWebFrameView] )
		{
			switch ( inCommand->commandID )
			{
				case kHICommandCut:
				case kHICommandCopy:
				case kHICommandPaste:
				case kHICommandClear:
				case kHICommandSelectAll:
					{
						SEL selector = _NSSelectorForHICommand( inCommand );
						if ( [respView respondsToSelector:selector] )
						{
							[respView performSelector:selector withObject:nil];
							result = noErr;
						}
					}
					break;
			}
		}
	}
	
	return result;
}

//----------------------------------------------------------------------------------
// UpdateCommandStatus
//----------------------------------------------------------------------------------
//
static OSStatus
UpdateCommandStatus( HIWebFrameView* inView, const HICommand* inCommand )
{
	OSStatus		result = eventNotHandledErr;
	MenuItemProxy* 	proxy = NULL;
	NSResponder*	resp;
	
	resp = [inView->fKitWindow firstResponder];
	
	if ( [resp isKindOfClass:[NSView class]] )
	{
		NSView*	respView = (NSView*)resp;

		if ( respView == inView->fWebFrameView
			|| [respView isDescendantOf: inView->fWebFrameView] )
		{
			if ( inCommand->attributes & kHICommandFromMenu )
			{
				SEL selector = _NSSelectorForHICommand( inCommand );
	
				if ( selector )
				{
					if ( [resp respondsToSelector: selector] )
					{
						proxy = [[MenuItemProxy alloc] initWithAction: selector];
						
						if ( [resp performSelector:@selector(validateUserInterfaceItem:) withObject: proxy] )
							EnableMenuItem( inCommand->menu.menuRef, inCommand->menu.menuItemIndex );
						else
							DisableMenuItem( inCommand->menu.menuRef, inCommand->menu.menuItemIndex );
						
						result = noErr;
					}
				}
			}
		}
	}
	
	if ( proxy )
		[proxy release];

	return result;
}

// Blatantly stolen from AppKit and cropped a bit

//----------------------------------------------------------------------------------
// _NSSelectorForHICommand
//----------------------------------------------------------------------------------
//
static SEL
_NSSelectorForHICommand( const HICommand* inCommand )
{
    switch ( inCommand->commandID )
	{
        case kHICommandUndo: return @selector(undo:);
        case kHICommandRedo: return @selector(redo:);
        case kHICommandCut  : return @selector(cut:);
        case kHICommandCopy : return @selector(copy:);
        case kHICommandPaste: return @selector(paste:);
        case kHICommandClear: return @selector(delete:);
        case kHICommandSelectAll: return @selector(selectAll:);
        default: return NULL;
    }

    return NULL;
}


//-----------------------------------------------------------------------------------
//	HIWebFrameViewEventHandler
//-----------------------------------------------------------------------------------
//	Our object's virtual event handler method. I'm not sure if we need this these days.
//	We used to do various things with it, but those days are long gone...
//
static OSStatus
HIWebFrameViewEventHandler(
	EventHandlerCallRef	inCallRef,
	EventRef			inEvent,
	void *				inUserData )
{
	OSStatus			result = eventNotHandledErr;
	HIPoint				where;
	OSType				tag;
	void *				ptr;
	Size				size;
	UInt32				features;
	RgnHandle			region = NULL;
	ControlPartCode		part;
	HIWebFrameView*			view = (HIWebFrameView*)inUserData;

	switch ( GetEventClass( inEvent ) )
	{
		case kEventClassHIObject:
			switch ( GetEventKind( inEvent ) )
			{
				case kEventHIObjectConstruct:
					{
						HIObjectRef		object;

						result = GetEventParameter( inEvent, kEventParamHIObjectInstance,
								typeHIObjectRef, NULL, sizeof( HIObjectRef ), NULL, &object );
						require_noerr( result, MissingParameter );
						
						// on entry for our construct event, we're passed the
						// creation proc we registered with for this class.
						// we use it now to create the instance, and then we
						// replace the instance parameter data with said instance
						// as type void.

						view = HIWebFrameViewConstructor( (HIViewRef)object );

						if ( view )
						{
							SetEventParameter( inEvent, kEventParamHIObjectInstance,
									typeVoidPtr, sizeof( void * ), &view ); 
						}
					}
					break;
				
				case kEventHIObjectDestruct:
					HIWebFrameViewDestructor( view );
					// result is unimportant
					break;
			}
			break;

		case kEventClassKeyboard:
			{
				CGSEventRecord		eventRec;
				NSEvent*			kitEvent;

				GetEventPlatformEventRecord( inEvent, &eventRec );
				RetainEvent( inEvent );
				kitEvent = [[NSEvent alloc] _initWithCGSEvent:(CGSEventRecord)eventRec eventRef:(void *)inEvent];

				[view->fKitWindow sendSuperEvent:kitEvent];
			
				result = noErr;
			}
			break;

		case kEventClassMouse:
			switch ( GetEventKind( inEvent ) )
			{
				case kEventMouseUp:
					result = MouseUp( view, inEvent );
					break;
				
				case kEventMouseWheelMoved:
					result = MouseWheelMoved( view, inEvent );
					break;

				case kEventMouseMoved:
					result = MouseMoved( view, inEvent );
					break;

				case kEventMouseDragged:
					result = MouseDragged( view, inEvent );
					break;
			}
			break;

		case kEventClassCommand:
			{
				HICommand		command;
				
				result = GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL,
								sizeof( HICommand ), NULL, &command );
				require_noerr( result, MissingParameter );
				
				switch ( GetEventKind( inEvent ) )
				{
					case kEventCommandProcess:
						result = ProcessCommand( view, &command );
						break;
					
					case kEventCommandUpdateStatus:
						result = UpdateCommandStatus( view, &command );
						break;
				}
			}
			break;

		case kEventClassControl:
			switch ( GetEventKind( inEvent ) )
			{
				case kEventControlInitialize:
					features = GetBehaviors();
					SetEventParameter( inEvent, kEventParamControlFeatures, typeUInt32,
							sizeof( UInt32 ), &features );
					result = noErr;
					break;
					
				case kEventControlDraw:
					{
						CGContextRef		context = NULL;
						
						GetEventParameter( inEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL,
								sizeof( RgnHandle ), NULL, &region );
						GetEventParameter( inEvent, kEventParamCGContextRef, typeCGContextRef, NULL,
								sizeof( CGContextRef ), NULL, &context );

						Draw( view, region, context );

						result = noErr;
					}
					break;
				
				case kEventControlHitTest:
					GetEventParameter( inEvent, kEventParamMouseLocation, typeHIPoint, NULL,
							sizeof( HIPoint ), NULL, &where );
					part = HitTest( view, &where );
					SetEventParameter( inEvent, kEventParamControlPart, typeControlPartCode, sizeof( ControlPartCode ), &part );
					result = noErr;
					break;
					
				case kEventControlGetPartRegion:
					GetEventParameter( inEvent, kEventParamControlPart, typeControlPartCode, NULL,
							sizeof( ControlPartCode ), NULL, &part );
					GetEventParameter( inEvent, kEventParamControlRegion, typeQDRgnHandle, NULL,
							sizeof( RgnHandle ), NULL, &region );
					result = GetRegion( view, part, region );
					break;
				
				case kEventControlGetData:
					GetEventParameter( inEvent, kEventParamControlPart, typeControlPartCode, NULL,
							sizeof( ControlPartCode ), NULL, &part );
					GetEventParameter( inEvent, kEventParamControlDataTag, typeEnumeration, NULL,
							sizeof( OSType ), NULL, &tag );
					GetEventParameter( inEvent, kEventParamControlDataBuffer, typePtr, NULL,
							sizeof( Ptr ), NULL, &ptr );
					GetEventParameter( inEvent, kEventParamControlDataBufferSize, typeLongInteger, NULL,
							sizeof( Size ), NULL, &size );
	
					if ( tag == kControlKindTag )
					{
						Size		outSize;
						
						result = noErr;

						if ( ptr )
						{
							if ( size != sizeof( ControlKind ) )
								result = errDataSizeMismatch;
							else
								( *(ControlKind *) ptr ) = GetKind();
						}

						outSize = sizeof( ControlKind );
						SetEventParameter( inEvent, kEventParamControlDataBufferSize, typeLongInteger, sizeof( Size ), &outSize );						
					}

					break;
				
				case kEventControlBoundsChanged:
					{
						HIRect		prevRect, currRect;
						UInt32		attrs;
						
						GetEventParameter( inEvent, kEventParamAttributes, typeUInt32, NULL,
								sizeof( UInt32 ), NULL, &attrs );
						GetEventParameter( inEvent, kEventParamOriginalBounds, typeHIRect, NULL,
								sizeof( HIRect ), NULL, &prevRect );
						GetEventParameter( inEvent, kEventParamCurrentBounds, typeHIRect, NULL,
								sizeof( HIRect ), NULL, &currRect );

						BoundsChanged( view, attrs, &prevRect, &currRect );
						result = noErr;
					}
					break;
				
				case kEventControlActivate:
					ActiveStateChanged( view );
					result = noErr;
					break;
					
				case kEventControlDeactivate:
					ActiveStateChanged( view );
					result = noErr;
					break;
															
				case kEventControlOwningWindowChanged:
					{
						WindowRef		fromWindow, toWindow;
						
						result = GetEventParameter( inEvent, kEventParamControlOriginalOwningWindow, typeWindowRef, NULL,
										sizeof( WindowRef ), NULL, &fromWindow );
						require_noerr( result, MissingParameter );

						result = GetEventParameter( inEvent, kEventParamControlCurrentOwningWindow, typeWindowRef, NULL,
										sizeof( WindowRef ), NULL, &toWindow );
						require_noerr( result, MissingParameter );

						OwningWindowChanged( view, fromWindow, toWindow );
						
						result = noErr;
					}
					break;
/*					
				case kEventControlDragEnter:
				case kEventControlDragLeave:
				case kEventControlDragWithin:
					{
						DragRef		drag;
						bool		likesDrag;
						
						inEvent.GetParameter( kEventParamDragRef, &drag );

						switch ( inEvent.GetKind() )
						{
							case kEventControlDragEnter:
								likesDrag = DragEnter( drag );
								
								// Why only if likesDrag?  What if it doesn't?  No parameter?
								if ( likesDrag )
									result = inEvent.SetParameter( kEventParamControlLikesDrag, likesDrag );
								break;
							
							case kEventControlDragLeave:
								DragLeave( drag );
								result = noErr;
								break;
							
							case kEventControlDragWithin:
								DragWithin( drag );
								result = noErr;
								break;
						}
					}
					break;
				
				case kEventControlDragReceive:
					{
						DragRef		drag;
						
						inEvent.GetParameter( kEventParamDragRef, &drag );

						result = DragReceive( drag );
					}
					break;
*/

				case kEventControlClick:
					result = Click( view, inEvent );
					break;
					
				case kEventControlContextualMenuClick:
					result = ContextMenuClick( view, inEvent );
					break;
					
				case kEventControlSetFocusPart:
					{
						ControlPartCode		desiredFocus;
						RgnHandle			invalidRgn;
						Boolean				focusEverything;
						ControlPartCode		actualFocus;
						
						result = GetEventParameter( inEvent, kEventParamControlPart, typeControlPartCode, NULL,
										sizeof( ControlPartCode ), NULL, &desiredFocus ); 
						require_noerr( result, MissingParameter );
						
						GetEventParameter( inEvent, kEventParamControlInvalRgn, typeQDRgnHandle, NULL,
								sizeof( RgnHandle ), NULL, &invalidRgn );

						focusEverything = false; // a good default in case the parameter doesn't exist

						GetEventParameter( inEvent, kEventParamControlFocusEverything, typeBoolean, NULL,
								sizeof( Boolean ), NULL, &focusEverything );

						result = SetFocusPart( view, desiredFocus, invalidRgn, focusEverything, &actualFocus );
						
						if ( result == noErr )
							verify_noerr( SetEventParameter( inEvent, kEventParamControlPart, typeControlPartCode,
									sizeof( ControlPartCode ), &actualFocus ) );
					}
					break;
				
				// some other kind of Control event
				default:
					break;
			}
			break;
			
		// some other event class
		default:
			break;
	}

MissingParameter:
	return result;
}

static void
StartUpdateObserver( HIWebFrameView* view )
{
	CFRunLoopObserverContext	context;
	CFRunLoopObserverRef		observer;
    CFRunLoopRef				mainRunLoop;
    
    check( view->fIsComposited == false );
    check( view->fUpdateObserver == NULL );

	context.version = 0;
	context.info = view;
	context.retain = NULL;
	context.release = NULL;
	context.copyDescription = NULL;

    mainRunLoop = (CFRunLoopRef)GetCFRunLoopFromEventLoop( GetMainEventLoop() );
	observer = CFRunLoopObserverCreate( NULL, kCFRunLoopEntry | kCFRunLoopBeforeWaiting, true, 0, UpdateObserver, &context );
	CFRunLoopAddObserver( mainRunLoop, observer, kCFRunLoopCommonModes ); 

    view->fUpdateObserver = observer;
    
//    printf( "Update observer started\n" );
}

static void
StopUpdateObserver( HIWebFrameView* view )
{
    check( view->fIsComposited == false );
    check( view->fUpdateObserver != NULL );

    CFRunLoopObserverInvalidate( view->fUpdateObserver );
    CFRelease( view->fUpdateObserver );
    view->fUpdateObserver = NULL;

//    printf( "Update observer removed\n" );
}

static void 
UpdateObserver( CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info )
{
	HIWebFrameView*			view = (HIWebFrameView*)info;
    RgnHandle				region = NewRgn();
    
//    printf( "Update observer called\n" );

    if ( region )
    {
        GetWindowRegion( GetControlOwner( view->fViewRef ), kWindowUpdateRgn, region );
        
        if ( !EmptyRgn( region ) )
        {
            RgnHandle		ourRgn = NewRgn();
            Rect			rect;
            
            GetWindowBounds( GetControlOwner( view->fViewRef ), kWindowStructureRgn, &rect );
            
//            printf( "Update region is non-empty\n" );
            
            if ( ourRgn )
            {
                Rect		rect;
                GrafPtr		savePort, port;
                Point		offset = { 0, 0 };
                
                port = GetWindowPort( GetControlOwner( view->fViewRef ) );
                
                GetPort( &savePort );
                SetPort( port );
                
                GlobalToLocal( &offset );
                OffsetRgn( region, offset.h, offset.v );

                GetControlBounds( view->fViewRef, &rect );
                RectRgn( ourRgn, &rect );
                
//                printf( "our control is at %d %d %d %d\n",
//                        rect.top, rect.left, rect.bottom, rect.right );
                
                GetRegionBounds( region, &rect );
//                printf( "region is at %d %d %d %d\n",
//                        rect.top, rect.left, rect.bottom, rect.right );

                SectRgn( ourRgn, region, ourRgn );
                
                GetRegionBounds( ourRgn, &rect );
//                printf( "intersection is  %d %d %d %d\n",
//                       rect.top, rect.left, rect.bottom, rect.right );
                if ( !EmptyRgn( ourRgn ) )
                {
                    RgnHandle	saveVis = NewRgn();
                    
//                    printf( "looks like we should draw\n" );

                    if ( saveVis )
                    {
//                        RGBColor	kRedColor = { 0xffff, 0, 0 };
                        
                        GetPortVisibleRegion( GetWindowPort( GetControlOwner( view->fViewRef ) ), saveVis );
                        SetPortVisibleRegion( GetWindowPort( GetControlOwner( view->fViewRef ) ), ourRgn );
                        
//                        RGBForeColor( &kRedColor );
//                        PaintRgn( ourRgn );
//                        QDFlushPortBuffer( port, NULL );
//                        Delay( 15, NULL );

                        Draw1Control( view->fViewRef );
                        ValidWindowRgn( GetControlOwner( view->fViewRef ), ourRgn );
                        
                        SetPortVisibleRegion( GetWindowPort( GetControlOwner( view->fViewRef ) ), saveVis );
                        DisposeRgn( saveVis );
                    }
                }

                SetPort( savePort );
                
                DisposeRgn( ourRgn );
            }
        }
        
        DisposeRgn( region );
    }
}
