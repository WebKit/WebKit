/*	IFWebView.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFImageView.h>
#import <WebKit/IFTextView.h>
#import <WebKit/IFWebView.h>
#import <WebKit/IFWebViewPrivate.h>

@implementation IFWebView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
    
    _private = [[IFWebViewPrivate alloc] init];

    IFDynamicScrollBarsView *scrollView  = [[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    return self;
}


- (void)dealloc 
{
    [_private release];
    [super dealloc];
}

- (void)setAllowsScrolling: (BOOL)flag
{
    [[self frameScrollView] setAllowsScrolling: flag];
}

- (BOOL)allowsScrolling
{
    return [[self frameScrollView] allowsScrolling];
}


- frameScrollView
{
    return _private->frameScrollView;
}   


- documentView
{
    return [[self frameScrollView] documentView];
}

// Note that the controller is not retained.
- (IFWebController *)controller
{
    return _private->controller;
}


- (BOOL) isDocumentHTML
{
    return [[[self documentView] className] isEqualToString:@"IFHTMLView"];
}

+ (void) registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType
{
    NSMutableDictionary *viewTypes = [[self class] _viewTypes];
        
    // FIXME: OK to allow developers to override built-in views?
    [viewTypes setObject:viewClass forKey:MIMEType];
}

+ (id <IFDocumentLoading>) createViewForMIMEType:(NSString *)MIMEType
{
    NSMutableDictionary *viewTypes = [[self class] _viewTypes];
    Class viewClass;
    NSArray *keys;
    unsigned i;
    
    viewClass = [viewTypes objectForKey:MIMEType];
    if(viewClass){
        return [[[viewClass alloc] initWithFrame:NSMakeRect(0,0,0,0)] autorelease];
    }else{
        keys = [viewTypes allKeys];
        for(i=0; i<[keys count]; i++){
            if([[keys objectAtIndex:i] hasSuffix:@"/"] && [MIMEType hasPrefix:[keys objectAtIndex:i]]){
                viewClass = [viewTypes objectForKey:[keys objectAtIndex:i]];
                return [[[viewClass alloc] initWithFrame:NSMakeRect(0,0,0,0)] autorelease];
            }
        }
    }
    return nil;
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)drawRect:(NSRect)rect
{
}


@end
