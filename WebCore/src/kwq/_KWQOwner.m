// This class is a temporary hack for our test apps.

#import "_KWQOwner.h"

@interface KWQHTMLView : NSView
- (void)setURL: (NSString *)urlString;
@end

@implementation _KWQOwner

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    NSLog (@"Did finish launching\n", nil);
}

- changeURL: sender
{
    // Get the 
    NSString *url = [sender stringValue];
    KWQHTMLView *htmlView;
    NSArray *subs;
    
    subs = [containerView subviews];
    htmlView = (KWQHTMLView *)[[subs objectAtIndex: 0] documentView];
    [htmlView setURL: url];
    [htmlView setNeedsLayout: YES];
    [containerView setNeedsDisplay: YES];
    
    return self;
}


@end
