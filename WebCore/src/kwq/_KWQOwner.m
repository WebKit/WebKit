// This class is a temporary hack for our test apps.

#import "_KWQOwner.h"

@interface KWQHTMLView : NSView
- (void)setURL: (NSString *)urlString;
@end

static BOOL flag = NO;

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
    
    if (!flag) {
        flag = YES;
        backForwardList = WCCreateBackForwardList();
        [self updateButtons];
        [throbber setUsesThreadedAnimation:YES];
        [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(uriClick:) name:@"uri-click" object:nil];
    }

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(newUriDone:) name:@"uri-done" object:nil];
    
    subs = [containerView subviews];
    htmlView = (KWQHTMLView *)[[subs objectAtIndex: 0] documentView];
    [htmlView setURL: url];
    [throbber startAnimation:self];
       
    return self;
}

-(void)back:(id)sender
{
    NSString *url;
    KWQHTMLView *htmlView;
    NSArray *subs;
    id <WCURIEntry> entry;
 
    entry = [backForwardList back];
    url = [[entry url] absoluteString];
    [urlBar setStringValue:url];

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(backForwardUriDone:) name:@"uri-done" object:nil];
    
    subs = [containerView subviews];
    htmlView = (KWQHTMLView *)[[subs objectAtIndex: 0] documentView];
    [htmlView setURL: url];
    [throbber startAnimation:self];
}

-(void)forward:(id)sender
{
    NSString *url;
    KWQHTMLView *htmlView;
    NSArray *subs;
    id <WCURIEntry> entry;
    
    entry = [backForwardList forward];
    url = [[entry url] absoluteString];
    [urlBar setStringValue:url];

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(backForwardUriDone:) name:@"uri-done" object:nil];
    
    subs = [containerView subviews];
    htmlView = (KWQHTMLView *)[[subs objectAtIndex: 0] documentView];
    [htmlView setURL: url];
    [throbber startAnimation:self];
}

-(void)uriClick:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(newUriDone:) name:@"uri-done" object:nil];
    [urlBar setStringValue:[notification object]];
    [throbber startAnimation:self];
}

-(void)backForwardUriDone:(NSNotification *)notification
{
    NSString *uriString;
    id <WCURIEntry> entry;
    
    uriString = [notification object];

    [[NSNotificationCenter defaultCenter] removeObserver:self 
        name:@"uri-done" object:nil];

    [self updateButtons];
    [throbber stopAnimation:self];
}

-(void)newUriDone:(NSNotification *)notification
{
    NSString *uriString;
    id <WCURIEntry> entry;
    
    uriString = [notification object];

    entry = WCCreateURIEntry();
    [entry setURL:[NSURL URLWithString:uriString]];
    [backForwardList addEntry: entry];

    [[NSNotificationCenter defaultCenter] removeObserver:self 
        name:@"uri-done" object:nil];

    [self updateButtons];
    [throbber stopAnimation:self];
}

-(void)updateButtons
{
    if ([backForwardList canGoBack]) {
        [backButton setEnabled: YES];
    }
    else {
        [backButton setEnabled: NO];
    }
    if ([backForwardList canGoForward]) {
        [forwardButton setEnabled: YES];
    }
    else {
        [forwardButton setEnabled: NO];
    }
}

@end
