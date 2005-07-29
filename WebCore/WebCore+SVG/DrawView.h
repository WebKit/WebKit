/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 WARNING: These files are temporary and exist solely for the purpose
 of allowing greater testing of WebCore+SVG prior to full DOM integration.
 Do NOT depend on these SPIs or files as they will soon be gone.
*/

#import <Cocoa/Cocoa.h>

@class DrawViewPrivate;
@class DrawDocument;

typedef enum {
    // viewing tools
    DrawViewToolBrowse, // click on links, interact w/ javascript, etc.
    DrawViewToolPan,
    DrawViewToolZoom,
    
    // editing tools
    DrawViewToolArrow, // select, change size, etc.
    DrawViewToolLine,
    DrawViewToolElipse,
    DrawViewToolRectangle,
    DrawViewToolTriangle, // generic polygon tool?
    DrawViewToolPolyLine,
    DrawViewToolArc,
    
} DrawViewTool;

@interface DrawView : NSView {
    
@private
    DrawViewTool _toolMode;
    BOOL	_isEditable;
    NSImageScaling _scaleRule;
    
    DrawViewPrivate *_private;
}

- (DrawDocument *)document;
- (void)setDocument:(DrawDocument *)document;

- (NSImageScaling)imageScaling;
- (void)setImageScaling:(NSImageScaling)scaling;

- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;
- (IBAction)zoomOriginal:(id)sender;
- (IBAction)zoomToFit:(id)sender;

// Will size the view to fit whatever SVG document it has in it.
- (void)sizeToFitViewBox;
// will fit whatever the canvas size is... (deprecated)
- (void)sizeToFitCanvas;

- (int)toolMode;
- (void)setToolMode:(int)toolMode;

/* Editing Support */

- (BOOL)isEditable;
- (void)setEditable:(BOOL)editable;

- (IBAction)deleteSelection:(id)sender;
- (IBAction)moveSelectionForward:(id)sender;
- (IBAction)moveSelectionBackward:(id)sender;

@end
