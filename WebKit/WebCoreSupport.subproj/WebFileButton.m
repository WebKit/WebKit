//
//  WebFileButton.m
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebFileButton.h"

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebCore/WebCoreViewFactory.h>
#import <WebKit/WebStringTruncator.h>

#define NO_FILE_SELECTED 

#define AFTER_BUTTON_SPACING 4
#define ICON_HEIGHT 16
#define ICON_WIDTH 16
#define ICON_FILENAME_SPACING 2
// FIXME: Is it OK to hard-code the width of the filename part?
#define FILENAME_WIDTH 200

#define ADDITIONAL_WIDTH (AFTER_BUTTON_SPACING + ICON_WIDTH + ICON_FILENAME_SPACING + FILENAME_WIDTH)

// We empirically determined that buttons have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define BUTTON_TOP_MARGIN 4
#define BUTTON_BOTTOM_MARGIN 6
#define BUTTON_LEFT_MARGIN 5
#define BUTTON_RIGHT_MARGIN 5

// AppKit calls this kThemePushButtonSmallTextOffset.
#define BUTTON_VERTICAL_FUDGE_FACTOR 2

@implementation WebFileButton

- (void)positionButton
{
    [_button sizeToFit];
    [_button setFrameOrigin:NSMakePoint(0, 0)];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _button = [[NSButton alloc] init];
        
        [_button setTitle:UI_STRING("Choose File", "title for file button used in HTML forms")];
        [[_button cell] setControlSize:NSSmallControlSize];
        [_button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
        [_button setBezelStyle:NSRoundedBezelStyle];
        [_button setTarget:self];
        [_button setAction:@selector(chooseButtonPressed:)];
        
        [self addSubview:_button];
        
        [self positionButton];
        [self setFilename:nil];
    }
    return self;
}

- (void)dealloc
{
    [_filename release];
    [_button release];
    [_icon release];
    [_label release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(NSIntersectionRect(bounds, rect));

    float left = bounds.size.width - ADDITIONAL_WIDTH + AFTER_BUTTON_SPACING;

    if (_icon) {
        float bottom = (bounds.size.height - BUTTON_BOTTOM_MARGIN - BUTTON_TOP_MARGIN - ICON_HEIGHT) / 2
            + BUTTON_BOTTOM_MARGIN;
        [_icon drawInRect:NSMakeRect(left, bottom, ICON_WIDTH, ICON_HEIGHT)
            fromRect:NSMakeRect(0, 0, [_icon size].width, [_icon size].height)
            operation:NSCompositeSourceOver fraction:1.0];
        left += ICON_WIDTH + ICON_FILENAME_SPACING;
    }

    NSFont *font = [_button font];
    NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
    [_label drawAtPoint:NSMakePoint(left, [self baseline]) withAttributes:attributes];

    [NSGraphicsContext restoreGraphicsState];
}

- (void)setFilename:(NSString *)filename
{
    NSString *copy = [filename copy];
    [_filename release];
    _filename = copy;
    
    [_label release];
    if (![_filename length]) {
        _label = [UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected") retain];
    } else {
        _label = [[WebStringTruncator centerTruncateString:
            [[NSFileManager defaultManager] displayNameAtPath:_filename]
            toWidth:FILENAME_WIDTH withFont:[_button font]] copy];
    }
    
    [_icon release];
    if (![_filename length]) {
        _icon = nil;
    } else {
        _icon = [[[NSWorkspace sharedWorkspace] iconForFile:_filename] retain];
    }
    
    [self setNeedsDisplay:YES];
}

- (NSString *)filename
{
    return [[_filename copy] autorelease];
}

- (void)setFrameSize:(NSSize)size
{
    // FIXME: Can we just use springs instead?
    [self positionButton];
    [super setFrameSize:size];
}

- (NSSize)bestVisualFrameSize
{
    NSSize size = [[_button cell] cellSize];
    size.height -= BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    size.width -= BUTTON_LEFT_MARGIN + BUTTON_RIGHT_MARGIN;
    size.width += ADDITIONAL_WIDTH;
    return size;
}

- (NSRect)visualFrame
{
    ASSERT([self superview] == nil || [[self superview] isFlipped]);
    NSRect frame = [self frame];
    frame.origin.x += BUTTON_LEFT_MARGIN;
    frame.size.width -= BUTTON_LEFT_MARGIN;
    frame.origin.y += BUTTON_BOTTOM_MARGIN;
    frame.size.height -= BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    return frame;
}

- (void)setVisualFrame:(NSRect)frame
{
    ASSERT([self superview] == nil || [[self superview] isFlipped]);
    frame.origin.x -= BUTTON_LEFT_MARGIN;
    frame.size.width += BUTTON_LEFT_MARGIN;
    frame.origin.y -= BUTTON_BOTTOM_MARGIN;
    frame.size.height += BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    [self setFrame:frame];
}

- (float)baseline
{
    // Button text is centered vertically, with a fudge factor to account for the shadow.
    ASSERT(_button);
    NSFont *buttonFont = [_button font];
    float ascender = [buttonFont ascender];
    float descender = [buttonFont descender];
    return ([[_button cell] cellSize].height - (ascender - descender)) / 2.0
        + BUTTON_VERTICAL_FUDGE_FACTOR + descender;
}

- (void)beginSheet
{
    [self retain];
    
    NSOpenPanel *sheet = [NSOpenPanel openPanel];    
    [sheet setPrompt:UI_STRING("Choose", "title for button in open panel from file button used in HTML forms")];
    [sheet beginSheetForDirectory:@"~" file:@"" types:nil
        modalForWindow:[self window] modalDelegate:self
        didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)chooseButtonPressed:(id)sender
{
    [self beginSheet];
}

- (void)mouseDown:(NSEvent *)event
{
    [self beginSheet];
}

- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSOKButton && [[sheet filenames] count] == 1) {
        [self setFilename:[[sheet filenames] objectAtIndex:0]];
        [[NSNotificationCenter defaultCenter] postNotificationName:WebCoreFileButtonFilenameChanged object:self];
    }
    [self release];
}

@end
