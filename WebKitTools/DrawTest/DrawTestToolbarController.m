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

#import "DrawTestToolbarController.h"

#import <WebCore/DrawViewPrivate.h>

enum
{
    ToolbarBrowseToolTag = 0,
    ToolbarPanToolTag,
    ToolbarZoomToolTag,
    
    ToolbarPointerToolTag,
    ToolbarLineToolTag,
    ToolbarRectangleToolTag,
    ToolbarElipseToolTag,
    ToolbarTriangleToolTag,
    ToolbarPolyLineToolTag,
    ToolbarArcToolTag,
    
    ToolbarDeleteSelectionTag,
    
    ToolbarMoveForwardTag,
    ToolbarMoveBackwardTag,
    ToolbarMoveToFrontTag,
    ToolbarMoveToBackTag,
    ToolbarMiscItem
};

// Constants
NSString *ToolbarIdentifier = @"Main Document Toolbar";

NSString *ToolbarBrowseToolIdentifier = @"Browse";
NSString *ToolbarPanToolIdentifier = @"Pan";
NSString *ToolbarZoomToolIdentifier = @"Zoom";

NSString *ToolbarPointerToolIdentifier = @"Pointer";
NSString *ToolbarRectangleToolIdentifier = @"Rectangle";
NSString *ToolbarElipseToolIdentifier = @"Oval";
NSString *ToolbarTriangleToolIdentifier = @"Triangle";
NSString *ToolbarPolyLineToolIdentifier = @"PolyLine";
NSString *ToolbarArcToolIdentifier = @"Arc";

NSString *ToolbarDeleteShapeIdentifier = @"Delete";
NSString *ToolbarMoveForwardIdentifier = @"Forward";
NSString *ToolbarMoveBackwardIdentifier = @"Backward";
NSString *ToolbarMoveToFrontIdentifier = @"Front";
NSString *ToolbarMoveToBackIdentifier = @"Back";

NSString *ToolbarPointerToolImage = @"Toolbar_Pointer";
NSString *ToolbarRectangleToolImage = @"Toolbar_Rectangle";
NSString *ToolbarElipseToolImage = @"Toolbar_Oval";
NSString *ToolbarTriangleToolImage = @"Toolbar_Triangle";

NSString *ToolbarDeleteShapeImage = @"Toolbar_Delete";
NSString *ToolbarMoveForwardImage = @"Toolbar_Forward";
NSString *ToolbarMoveBackwardImage = @"Toolbar_Backward";
NSString *ToolbarMoveToFrontImage = @"Toolbar_Front";
NSString *ToolbarMoveToBackImage = @"Toolbar_Back";

@interface DrawTestToolbarController (InternalMethods)
- (void)setupToolbar;
- (void)addToolbarItemWithIdentifier:(NSString *)identifier withImage:(NSString *)image withTag:(int)tag;
- (void)addToolbarItemWithIdentifier:(NSString *)identifier withImage:(NSString *)image;
- (void)addToolbarItem:(NSString *)identifier
             withLabel:(NSString *)label
      withPaletteLabel:(NSString *)paletteLabel
             withImage:(NSString *)imageName
           withToolTip:(NSString *)toolTip
               withTag:(int)tag;
@end


@implementation DrawTestToolbarController

- (id)initWithDrawView:(DrawView *)drawView
{
    if (self = [super init]){
        _drawView = [drawView retain];
        [self setupToolbar]; // could be done lazily.
    }
    return self;
}

- (void)dealloc
{
    [_toolbarItems release];
    [super dealloc];
}


- (void)addToolbarItemWithIdentifier:(NSString *)identifier withImage:(NSString *)image withTag:(int)tag
{
    
    [self addToolbarItem:identifier
               withLabel:identifier
        withPaletteLabel:identifier
               withImage:image
             withToolTip:identifier
                 withTag:tag];
}

- (void)addToolbarItemWithIdentifier:(NSString *)identifier withImage:(NSString *)image
{
    [self addToolbarItemWithIdentifier:identifier withImage:image withTag:ToolbarMiscItem];
}

- (void)addToolbarItem:(NSString *)identifier
             withLabel:(NSString *)label
      withPaletteLabel:(NSString *)paletteLabel
             withImage:(NSString *)imageName
           withToolTip:(NSString *)toolTip
               withTag:(int)tag
{
    NSToolbarItem *item = [[[NSToolbarItem alloc] initWithItemIdentifier:identifier] autorelease];
    
    [item setLabel:label];
    [item setPaletteLabel:paletteLabel];
    [item setToolTip:toolTip];
    [item setImage:[NSImage imageNamed:imageName]];
    [item setTarget:self];
    [item setAction:@selector(clickedToolbarItem:)];
    [item setTag:tag];
    
    [_toolbarItems setObject:item forKey:identifier];
}

- (void)setupToolbar
{
    _toolbarItems = [[NSMutableDictionary alloc] init];
    
    [self addToolbarItemWithIdentifier:ToolbarBrowseToolIdentifier 
                             withImage:ToolbarPointerToolImage
                               withTag:ToolbarBrowseToolTag];
    [[_toolbarItems objectForKey:ToolbarBrowseToolIdentifier] setImage:[[NSCursor pointingHandCursor] image]];
    
    [self addToolbarItemWithIdentifier:ToolbarPanToolIdentifier 
                             withImage:ToolbarPointerToolImage
                               withTag:ToolbarPanToolTag];
    [[_toolbarItems objectForKey:ToolbarPanToolIdentifier] setImage:[[NSCursor openHandCursor] image]];
    
    [self addToolbarItemWithIdentifier:ToolbarZoomToolIdentifier 
                             withImage:ToolbarPointerToolImage
                               withTag:ToolbarZoomToolTag];
    
    
    [self addToolbarItemWithIdentifier:ToolbarPointerToolIdentifier 
                             withImage:ToolbarPointerToolImage
                               withTag:ToolbarPointerToolTag];
    
    [self addToolbarItemWithIdentifier:ToolbarRectangleToolIdentifier 
                             withImage:ToolbarRectangleToolImage
                               withTag:ToolbarRectangleToolTag];
    
    [self addToolbarItemWithIdentifier:ToolbarElipseToolIdentifier 
                             withImage:ToolbarElipseToolImage 
                               withTag:ToolbarElipseToolTag];
    
    [self addToolbarItemWithIdentifier:ToolbarTriangleToolIdentifier 
                             withImage:ToolbarTriangleToolImage
                               withTag:ToolbarTriangleToolTag];
    
    [self addToolbarItemWithIdentifier:ToolbarDeleteShapeIdentifier 
                             withImage:ToolbarDeleteShapeImage
                               withTag:ToolbarDeleteSelectionTag];
    
    [self addToolbarItemWithIdentifier:ToolbarMoveForwardIdentifier 
                             withImage:ToolbarMoveForwardImage
                               withTag:ToolbarMoveForwardTag];
    
    [self addToolbarItemWithIdentifier:ToolbarMoveBackwardIdentifier 
                             withImage:ToolbarMoveBackwardImage
                               withTag:ToolbarMoveBackwardTag];
    
    [self addToolbarItemWithIdentifier:ToolbarMoveToFrontIdentifier 
                             withImage:ToolbarMoveToFrontImage
                               withTag:ToolbarMoveToFrontTag];
    
    [self addToolbarItemWithIdentifier:ToolbarMoveToBackIdentifier 
                             withImage:ToolbarMoveToBackImage
                               withTag:ToolbarMoveToBackTag];
    
    
    NSToolbar *toolbar = [[[NSToolbar alloc] initWithIdentifier:ToolbarIdentifier] autorelease];
    
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [toolbar setDisplayMode:NSToolbarDisplayModeIconAndLabel];
    [toolbar setSizeMode:NSToolbarSizeModeSmall];
    [toolbar setDisplayMode:NSToolbarDisplayModeLabelOnly];
    [toolbar setDelegate:self];
    [toolbar setSelectedItemIdentifier:ToolbarBrowseToolIdentifier];
    [toolbar setVisible:NO];
    
    [[_drawView window] setToolbar:toolbar];
}


- (void)clickedToolbarItem:(id)sender
{
    int tag = [sender tag];
    
    switch(tag) {
        case ToolbarBrowseToolTag:
            [_drawView setToolMode:DrawViewToolBrowse];
            break;
        case ToolbarPanToolTag:
            [_drawView setToolMode:DrawViewToolPan];
            break;
        case ToolbarZoomToolTag:
            [_drawView setToolMode:DrawViewToolZoom];
            break;
        case ToolbarPointerToolTag:
            [_drawView setToolMode:DrawViewToolArrow];
            break;
        case ToolbarRectangleToolTag:
            [_drawView setToolMode:DrawViewToolRectangle];
            break;
        case ToolbarElipseToolTag:
            [_drawView setToolMode:DrawViewToolElipse];
            break;
        case ToolbarTriangleToolTag:
            [_drawView setToolMode:DrawViewToolTriangle];
            break;
        case ToolbarMoveForwardTag:
            [_drawView moveSelectionForward:sender];
            break;
        case ToolbarMoveBackwardTag:
            [_drawView moveSelectionBackward:sender];
            break;
        case ToolbarDeleteSelectionTag:
            [_drawView deleteSelection:sender];
            break;
        default:
            NSLog(@"Toolbar item: %i not implemented!", tag);
    }
    
    [_drawView setNeedsDisplay:YES];
}

// NSToolbar delegate methods
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdent 
 willBeInsertedIntoToolbar:(BOOL)willBeInserted
{ 
    NSToolbarItem *    toolbarItem = [_toolbarItems objectForKey:itemIdent];
    
    if( toolbarItem == nil )
        toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier:itemIdent] autorelease];
    
    return toolbarItem;
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
        ToolbarBrowseToolIdentifier,
        ToolbarPanToolIdentifier,
        ToolbarZoomToolIdentifier,
        ToolbarPointerToolIdentifier,
        ToolbarRectangleToolIdentifier,
        ToolbarElipseToolIdentifier,
        ToolbarTriangleToolIdentifier,
        ToolbarDeleteShapeIdentifier, 
        NSToolbarShowColorsItemIdentifier,
        ToolbarMoveForwardIdentifier,
        ToolbarMoveBackwardIdentifier,
        ToolbarMoveToFrontIdentifier,
        ToolbarMoveToBackIdentifier,
        NSToolbarCustomizeToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier, 
        NSToolbarSpaceItemIdentifier, 
        NSToolbarSeparatorItemIdentifier, nil];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{    
    return [NSArray arrayWithObjects:
        ToolbarBrowseToolIdentifier,
        ToolbarPanToolIdentifier,
        //ToolbarZoomToolIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier,
        ToolbarPointerToolIdentifier,
        ToolbarRectangleToolIdentifier,
        ToolbarElipseToolIdentifier,
        //ToolbarTriangleToolIdentifier, 
        //NSToolbarSeparatorItemIdentifier,
        ToolbarDeleteShapeIdentifier, 
        //ToolbarMoveForwardIdentifier,
        //ToolbarMoveBackwardIdentifier,
        //ToolbarMoveToFrontIdentifier,
        //ToolbarMoveToBackIdentifier, 
        NSToolbarSeparatorItemIdentifier,
        //NSToolbarShowColorsItemIdentifier, 
        //NSToolbarFlexibleSpaceItemIdentifier, 
        NSToolbarCustomizeToolbarItemIdentifier,
        nil];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
        ToolbarBrowseToolIdentifier,
        ToolbarPanToolIdentifier,
        ToolbarZoomToolIdentifier,    
        ToolbarPointerToolIdentifier,
        ToolbarRectangleToolIdentifier,
        ToolbarElipseToolIdentifier,
        ToolbarTriangleToolIdentifier,
        nil];
}

- (BOOL)validateToolbarItem:(NSToolbarItem *)theItem
{
    BOOL enabled = YES;
    
    switch([theItem tag]) {
        case ToolbarMoveForwardTag:
        case ToolbarMoveBackwardTag:
        case ToolbarMoveToFrontTag:
        case ToolbarMoveToBackTag:
        case ToolbarDeleteSelectionTag:
            enabled = ([[_drawView selectedCanvasItems] count] != 0);
    }
    
    return enabled;
}

@end
