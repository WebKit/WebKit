/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebPreferences.h>

@interface NSString (NSStringTextFinding)

- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)mask wrap:(BOOL)wrapFlag;

@end

@implementation NSString (_Web_StringTextFinding)

- (NSRange)findString:(NSString *)string selectedRange:(NSRange)selectedRange options:(unsigned)options wrap:(BOOL)wrap {
    BOOL forwards = (options & NSBackwardsSearch) == 0;
    unsigned length = [self length];
    NSRange searchRange, range;

    if (forwards) {
	searchRange.location = NSMaxRange(selectedRange);
	searchRange.length = length - searchRange.location;
	range = [self rangeOfString:string options:options range:searchRange];
        if ((range.length == 0) && wrap) {	/* If not found look at the first part of the string */
	    searchRange.location = 0;
            searchRange.length = selectedRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
    } else {
	searchRange.location = 0;
	searchRange.length = selectedRange.location;
        range = [self rangeOfString:string options:options range:searchRange];
        if ((range.length == 0) && wrap) {
            searchRange.location = NSMaxRange(selectedRange);
            searchRange.length = length - searchRange.location;
            range = [self rangeOfString:string options:options range:searchRange];
        }
    }
    return range;
}        

@end

@implementation WebTextView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
        [[self textContainer] setWidthTracksTextView:YES];
        [self setAutoresizingMask:NSViewWidthSizable];
        [self setEditable:NO];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(defaultsChanged:)
                                                     name:NSUserDefaultsDidChangeNotification
                                                   object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)setFixedWidthFont
{
    WebPreferences *preferences = [WebPreferences standardPreferences];
    NSFont *font = [NSFont fontWithName:[preferences fixedFontFamily] size:[preferences defaultFontSize]];
    [self setFont:font];
}

- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource
{
}

- (void)provisionalDataSourceCommitted:(WebDataSource *)dataSource
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
    NSString *string;
    
    // FIXME: This needs to be more efficient for progressively loading documents.
    
    if ([[dataSource contentType] isEqualToString:@"text/rtf"]) {
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0,0) withRTF:[dataSource data]];
    } else {
        [self setRichText:NO];
        [self setFixedWidthFont];
        // FIXME: This needs to use the correct encoding, but the list of names of encodings
        // is currently inside WebCore where we can't share it.
        string = [[NSString alloc] initWithData:[dataSource data] encoding:NSASCIIStringEncoding];
        [self setString:string];
        [string release];
    }
}

- (void)layout
{
    NSRect superFrame = [[self superview] frame];
    NSRect frame = [self frame];
    
    [self setFrame:NSMakeRect(frame.origin.x, frame.origin.y, superFrame.size.width, frame.size.height)];
}

- (void)setCanDragFrom: (BOOL)flag
{
    canDragFrom = flag;
}

- (BOOL)canDragFrom
{
    return canDragFrom;
}

- (void)setCanDragTo: (BOOL)flag
{
    canDragTo = flag;
}

- (BOOL)canDragTo
{
    return canDragTo;
}

- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    BOOL lastFindWasSuccessful = NO;
    NSString *textContents = [self string];
    unsigned textLength;
    
    if (textContents && (textLength = [textContents length])) {
        NSRange range;
        unsigned options = 0;
        
        if (!forward) 
            options |= NSBackwardsSearch;
            
        if (!caseFlag)
            options |= NSCaseInsensitiveSearch;
            
        range = [textContents findString:string selectedRange:[self selectedRange] options:options wrap:YES];
        if (range.length) {
            [self setSelectedRange:range];
            [self scrollRangeToVisible:range];
            lastFindWasSuccessful = YES;
        }
    }

    return lastFindWasSuccessful;
}

- (void)defaultsChanged:(NSNotification *)notification
{
    if(![self isRichText]){
        [self setFixedWidthFont];
    }
}

- (BOOL)canTakeFindStringFromSelection
{
    return [self isSelectable] && [self selectedRange].length && ![self hasMarkedText];
}


- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self canTakeFindStringFromSelection]) {
        NSBeep();
        return;
    }
    
    // Note: can't use writeSelectionToPasteboard:type: here, though it seems equivalent, because
    // it doesn't declare the types to the pasteboard and thus doesn't bump the change count
    [self writeSelectionToPasteboard:[NSPasteboard pasteboardWithName:NSFindPboard]
                               types:[NSArray arrayWithObject:NSStringPboardType]];
}


@end
