//
//  NSHIViewAdapter.m
//  Synergy
//
//  Created by Ed Voas on Mon Jan 20 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "HIViewAdapter.h"

extern OSStatus		_HIViewSetNeedsDisplayInRect( HIViewRef inView, const HIRect* inBounds, Boolean inNeedsDisplay );

#define SHEET_INTERCEPTOR 0

@interface NSView(ShhhhDontTell)
- (NSRect)_dirtyRect;
@end

#if SHEET_INTERCEPTOR
@interface CarbonSheetInterceptor : NSWindow
{
}

- (void)_orderFrontRelativeToWindow:(NSWindow *)docWindow;
@end
#endif

@implementation HIViewAdapter

static CFMutableDictionaryRef	sViewMap;

+ (void)bindHIViewToNSView:(HIViewRef)hiView nsView:(NSView*)nsView
{
	if ( sViewMap == NULL )
	{
		[HIViewAdapter poseAsClass: [NSView class]];
#if SHEET_INTERCEPTOR
		[CarbonSheetInterceptor poseAsClass: [NSWindow class]];
#endif
		sViewMap = CFDictionaryCreateMutable( NULL, 0, NULL, NULL );
	}
	
	CFDictionaryAddValue( sViewMap, nsView, hiView );
}

+ (HIViewRef)getHIViewForNSView:(NSView*)inView
{
	return sViewMap ? (HIViewRef)CFDictionaryGetValue( sViewMap, inView ) : NULL;
}

+ (void)unbindNSView:(NSView*)inView
{
	CFDictionaryRemoveValue( sViewMap, inView );
}

- (id)initWithFrame:(NSRect)frame view:(HIViewRef) inView {
    self = [super initWithFrame:frame];
//    if (self) {
//       _hiView = inView;
//    }
    return self;
}

#if 0
- (HIViewRef)hiView {
	return _hiView;
}

- (NSView*)nextValidKeyView {
	//fprintf( stderr, "spoofing next valid key view\n" );
	return [_window contentView];
}
#endif

- (void)setNeedsDisplay:(BOOL)flag {
	[super setNeedsDisplay:flag];
	
	if ( flag == NO )
	{
    	HIViewRef	hiView = NULL;
    	NSRect		targetBounds = [self visibleRect];
		NSRect		validRect = targetBounds;
    	NSView*		view = self;
    	
    	while ( view ) {
			targetBounds = [view visibleRect];
			if ( (hiView = [HIViewAdapter getHIViewForNSView:view]) != NULL )
				break;
			validRect = [view convertRect:validRect toView:[view superview]];
    		view = [view superview];
		}
    	
    	if ( hiView )
		{
			HIRect		rect;
			
			//printf( "validating %g %g %g %g\n", validRect.origin.y, validRect.origin.x,
			//	NSMaxY( validRect ), NSMaxX( validRect ) );
    		
    		//printf( "targetBounds.size.height = %g, NSMaxY( validRect ) = %g\n", targetBounds.size.height, NSMaxY( validRect ) );
			
			// flip rect here and convert to region
			
			rect.origin.x 		= validRect.origin.x;
			rect.origin.y 		= targetBounds.size.height - NSMaxY( validRect );
			rect.size.height 	= validRect.size.height;
			rect.size.width 	= validRect.size.width;

			//printf( "  flipped to %g %g %g %g\n", rect.origin.y, rect.origin.x,
			//	CGRectGetMaxY( rect ), CGRectGetMaxX( rect ) );

			// For now, call the region-based API.
			{
				RgnHandle rgn = NewRgn();
				if ( rgn )
				{
					Rect		qdRect;
					qdRect.top = (SInt16)rect.origin.y;
					qdRect.left = (SInt16)rect.origin.x;
					qdRect.bottom = CGRectGetMaxY( rect );
					qdRect.right = CGRectGetMaxX( rect );
				
					RectRgn( rgn, &qdRect );
					HIViewSetNeedsDisplayInRegion( hiView, rgn, false );
					DisposeRgn( rgn );
				}
				else
				{
					HIViewSetNeedsDisplay( hiView, false );
				}
			}
			//_HIViewSetNeedsDisplayInRect( hiView, &rect, false );
		}
	}
}

- (void)setNeedsDisplayInRect:(NSRect)invalidRect {
	[super setNeedsDisplayInRect:invalidRect];
	
    if (_vFlags.needsDisplay || _vFlags.needsDisplayForBounds) 
    {
    	HIViewRef	hiView = NULL;
    	NSRect		targetBounds = _bounds;
    	NSView*		view = self;
    	
		invalidRect = [self _dirtyRect];
		
    	while ( view ) {
			targetBounds = [view bounds];
			if ( (hiView = [HIViewAdapter getHIViewForNSView:view]) != NULL )
				break;
			invalidRect = [view convertRect:invalidRect toView:[view superview]];
    		view = [view superview];
		}
    	
    	if ( hiView )
    	{
    		if ( NSWidth( invalidRect ) > 0 && NSHeight( invalidRect ) )
    		{
    			HIRect		rect;
    			
    			//printf( "invalidating %g %g %g %g\n", invalidRect.origin.y, invalidRect.origin.x,
    			//	NSMaxY( invalidRect ), NSMaxX( invalidRect ) );
    		
    			//printf( "targetBounds.size.height = %g, NSMaxY( invalidRect ) = %g\n", targetBounds.size.height, NSMaxY( invalidRect ) );
    			
    			// flip rect here and convert to region
    			
				rect.origin.x 		= invalidRect.origin.x;
				rect.origin.y 		= targetBounds.size.height - NSMaxY( invalidRect );
				rect.size.height 	= invalidRect.size.height;
				rect.size.width 	= invalidRect.size.width;

    			//printf( "  flipped to %g %g %g %g\n", rect.origin.y, rect.origin.x,
    			//	CGRectGetMaxY( rect ), CGRectGetMaxX( rect ) );

				// For now, call the region-based API.
				{
					RgnHandle rgn = NewRgn();
					if ( rgn )
					{
						Rect		qdRect;
						qdRect.top = (SInt16)rect.origin.y;
						qdRect.left = (SInt16)rect.origin.x;
						qdRect.bottom = CGRectGetMaxY( rect );
						qdRect.right = CGRectGetMaxX( rect );
					
						RectRgn( rgn, &qdRect );
						HIViewSetNeedsDisplayInRegion( hiView, rgn, true );
						DisposeRgn( rgn );
					}
					else
					{
						HIViewSetNeedsDisplay( hiView, true );
					}
				}
	    		//_HIViewSetNeedsDisplayInRect( hiView, &rect, true );
	    	}
    	}
    	else
    		[_window setViewsNeedDisplay:YES];
	}
}

- (NSView *)nextValidKeyView
{
	HIViewRef		hiView = [HIViewAdapter getHIViewForNSView:self];
	NSView*			retVal = NULL;

	if ( hiView )
	{
		//fprintf( stderr, "spoofing next valid key view\n" );
		retVal = [_window contentView];
	}
	else
	{
		retVal = [super nextValidKeyView];
	}
	
	return retVal;
}

@end

#if SHEET_INTERCEPTOR
@implementation CarbonSheetInterceptor

- (void)_orderFrontRelativeToWindow:(NSWindow *)docWindow
{
	printf( "Here I am!\n" );
    [docWindow _attachSheetWindow:self];	
	SetWindowClass( [self windowRef], kSheetWindowClass );
	ShowSheetWindow( [self windowRef], [docWindow windowRef] );
}

- (void)_orderOutRelativeToWindow:(NSWindow *)docWindow
{
	printf( "Here I go!\n" );
	HideSheetWindow( [self windowRef] );
    [docWindow _detachSheetWindow];
}

@end
#endif
