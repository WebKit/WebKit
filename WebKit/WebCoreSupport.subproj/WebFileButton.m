//
//  WebFileButton.m
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebFileButton.h"

#import <WebFoundation/WebLocalizableStrings.h>

#import <WebCore/WebCoreViewFactory.h>

#import <WebKit/WebStringTruncator.h>

#define NO_FILE_SELECTED 

#define AFTER_BUTTON_SPACING 4
#define ICON_HEIGHT 16
#define ICON_WIDTH 16
#define ICON_FILENAME_SPACING 2
// FIXME: Is it OK to hard-code the width of the filename part of this control?
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
    // FIXME: Need to take margins into account.
    [_button setFrameOrigin:NSMakePoint(0, 0)];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _button = [[NSButton alloc] init];
        [_button setTitle:UI_STRING("Choose File", "title for file button used in HTML forms")];
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

- (NSFont *)font
{
    return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(NSIntersectionRect(bounds, rect));

    float left = bounds.size.width - ADDITIONAL_WIDTH + AFTER_BUTTON_SPACING;

    if (_icon) {
        [_icon drawInRect:NSMakeRect(left, (bounds.size.height - ICON_HEIGHT) / 2, ICON_WIDTH, ICON_HEIGHT)
            fromRect:NSMakeRect(0, 0, [_icon size].width, [_icon size].height)
            operation:NSCompositeSourceOver fraction:1.0];
        left += ICON_WIDTH + ICON_FILENAME_SPACING;
    }

    NSFont *font = [self font];
    NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
    [_label drawAtPoint:NSMakePoint(left, [self baseline] - [font ascender]) withAttributes:attributes];

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
            toWidth:FILENAME_WIDTH withFont:[self font]] copy];
    }
    
    [_icon release];
    _icon = [[[NSWorkspace sharedWorkspace] iconForFile:_filename] retain];
    
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

- (NSSize)bestSize
{
    // FIXME: Not yet implemented
    return [self frame].size;
}

- (float)baseline
{
    // FIXME: Not yet implemented
    return 0;
}

- (void)chooseButtonPressed:(id)sender
{
    [self retain];
    
    NSOpenPanel *sheet = [NSOpenPanel openPanel];    
    [sheet setPrompt:UI_STRING("Choose", "title for button in open panel from file button used in HTML forms")];
    [sheet beginSheetForDirectory:@"~" file:@"" types:nil
        modalForWindow:[self window] modalDelegate:self
        didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSOKButton && [[sheet filenames] count] == 1) {
        [self setFilename:[[sheet filenames] objectAtIndex:0]];
    }
    [self release];
}

@end
