/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "HIViewAdapter.h"

//extern OSStatus		_HIViewSetNeedsDisplayInRect( HIViewRef inView, const HIRect* inBounds, Boolean inNeedsDisplay );
static void			SetViewNeedsDisplay( HIViewRef inView, RgnHandle inRegion, Boolean inNeedsDisplay );

#define WATCH_INVALIDATION 		0

@interface NSView(ShhhhDontTell)
- (NSRect)_dirtyRect;
@end

@implementation HIViewAdapter

static CFMutableDictionaryRef	sViewMap;

+ (void)bindHIViewToNSView:(HIViewRef)hiView nsView:(NSView*)nsView
{
	if ( sViewMap == NULL )
	{
		[HIViewAdapter poseAsClass: [NSView class]];
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
					SetViewNeedsDisplay( hiView, rgn, false );
					DisposeRgn( rgn );
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
						SetViewNeedsDisplay( hiView, rgn, true );
						DisposeRgn( rgn );
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

static void
SetViewNeedsDisplay( HIViewRef inHIView, RgnHandle inRegion, Boolean inNeedsDisplay )
{
	WindowAttributes		attrs;
	
	GetWindowAttributes( GetControlOwner( inHIView ), &attrs );

	if ( attrs & kWindowCompositingAttribute )
	{
#if WATCH_INVALIDATION
		Rect		bounds;
		GetRegionBounds( inRegion, &bounds );
		printf( "%s: rect on input %d %d %d %d\n", inNeedsDisplay ? "INVALIDATE" : "VALIDATE",
			bounds.top, bounds.left, bounds.bottom, bounds.right );
#endif
		HIViewSetNeedsDisplayInRegion( inHIView, inRegion, inNeedsDisplay );
	}
	else
	{
		Rect			bounds, cntlBounds;
		GrafPtr			port, savePort;
		Rect			portBounds;
		
#if WATCH_INVALIDATION
		printf( "%s: rect on input %d %d %d %d\n", inNeedsDisplay ? "INVALIDATE" : "VALIDATE",
			bounds.top, bounds.left, bounds.bottom, bounds.right );
#endif
		GetControlBounds( inHIView, &cntlBounds );

#if WATCH_INVALIDATION
		printf( "%s: control bounds are %d %d %d %d\n", inNeedsDisplay ? "INVALIDATE" : "VALIDATE",
			cntlBounds.top, cntlBounds.left, cntlBounds.bottom, cntlBounds.right );
#endif

		port = GetWindowPort( GetControlOwner( inHIView ) );
		
		GetPort( &savePort );
		SetPort( port );
		GetPortBounds( port, &portBounds );
		SetOrigin( 0, 0 );
		
		OffsetRgn( inRegion, cntlBounds.left, cntlBounds.top );

		GetRegionBounds( inRegion, &bounds );

#if WATCH_INVALIDATION
		printf( "%s: rect in port coords %d %d %d %d\n", inNeedsDisplay ? "INVALIDATE" : "VALIDATE",
			bounds.top, bounds.left, bounds.bottom, bounds.right );
#endif

		if ( inNeedsDisplay )
			InvalWindowRgn( GetControlOwner( inHIView ), inRegion );
		else
			ValidWindowRgn( GetControlOwner( inHIView ), inRegion );
		
		SetOrigin( portBounds.left, portBounds.top );
		SetPort( savePort );
	}
}

