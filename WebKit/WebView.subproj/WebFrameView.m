/*	IFWebView.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFImageView.h>
#import <WebKit/IFTextView.h>
#import <WebKit/IFWebView.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreViewFactory.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFTextRendererFactory.h>
#import <WebKit/IFImageRendererFactory.h>

#import <WebFoundation/IFNSStringExtensions.h>

@implementation IFWebView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
 
    [IFWebCoreViewFactory createSharedFactory];
    [IFTextRendererFactory createSharedFactory];
    [IFImageRendererFactory createSharedFactory];
   
    _private = [[IFWebViewPrivate alloc] init];

    IFDynamicScrollBarsView *scrollView  = [[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    _private->draggingTypes = [[NSArray arrayWithObjects:@"NSFilenamesPboardType", 
                                    @"NSURLPboardType", @"NSStringPboardType", nil] retain];
    [self registerForDraggedTypes:_private->draggingTypes];
    
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


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSString *dragType, *file, *URLString;
    NSArray *files;
        
    dragType = [[sender draggingPasteboard] availableTypeFromArray:_private->draggingTypes];
    if([dragType isEqualToString:@"NSFilenamesPboardType"]){
        files = [[sender draggingPasteboard] propertyListForType:@"NSFilenamesPboardType"];
        
        // FIXME: We only look at the first dragged file (2931225)
        file = [files objectAtIndex:0];
        
        if([IFWebController canShowFile:file])
            return NSDragOperationCopy;
            
    }else if([dragType isEqualToString:@"NSURLPboardType"]){
        //use URLFromPasteboard:
        return NSDragOperationCopy;
    }else if([dragType isEqualToString:@"NSStringPboardType"]){
        URLString = [[sender draggingPasteboard] stringForType:@"NSStringPboardType"];
        if([URLString _IF_looksLikeAbsoluteURL])
            return NSDragOperationCopy;
    }
    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    IFWebDataSource *dataSource;
    IFWebFrame *frame;
    NSArray *files;
    NSString *file, *dragType;
    NSURL *URL=nil;
    
    dragType = [[sender draggingPasteboard] availableTypeFromArray:_private->draggingTypes];
    if([dragType isEqualToString:@"NSFilenamesPboardType"]){
        files = [[sender draggingPasteboard] propertyListForType:@"NSFilenamesPboardType"];
        file = [files objectAtIndex:0];
        URL = [NSURL fileURLWithPath:file];
    }else if([dragType isEqualToString:@"NSURLPboardType"]){
        // FIXME: Is this the right way to get the URL? How to test?
        URL = [NSURL URLWithString:[[sender draggingPasteboard] stringForType:@"NSURLPboardType"]];
    }else if([dragType isEqualToString:@"NSStringPboardType"]){
        URL = [NSURL URLWithString:[[sender draggingPasteboard] stringForType:@"NSStringPboardType"]];
    }
    
    if(!URL)
        return NO;
        
    dataSource = [[[IFWebDataSource alloc] initWithURL:URL] autorelease];
    frame = nil;
    frame = [[self controller] mainFrame];
    [frame setProvisionalDataSource:dataSource];
    [frame startLoading];
    
    return YES;
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
