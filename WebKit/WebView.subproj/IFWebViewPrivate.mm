/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFImageView.h>
#import <WebKit/IFTextView.h>

static NSMutableDictionary *_viewTypes=nil;

@implementation IFWebViewPrivate

- init
{
    [super init];
    
    marginWidth = -1;
    marginHeight = -1;
    
    return self;
}

- (void)dealloc
{
    [frameScrollView release];
    [super dealloc];
}

@end

@implementation IFWebView (IFPrivate)

- (void)_setMarginWidth: (int)w
{
    _private->marginWidth = w;
}

- (int)_marginWidth
{
    return _private->marginWidth;
}

- (void)_setMarginHeight: (int)h
{
    _private->marginHeight = h;
}

- (int)_marginHeight
{
    return _private->marginHeight;
}

- (void)_setDocumentView:(id)view
{
    [[self frameScrollView] setDocumentView: view];    
}


- (void)_setController: (IFWebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}


+ (NSMutableDictionary *)_viewTypes
{
    if(!_viewTypes){
        _viewTypes = [[NSMutableDictionary dictionary] retain];
        [_viewTypes setObject:[IFHTMLView class]  forKey:@"text/html"];
        [_viewTypes setObject:[IFTextView class]  forKey:@"text/"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/jpeg"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/gif"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/png"];
    }
    return _viewTypes;
}


+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    NSMutableDictionary *viewTypes = [[self class] _viewTypes];
    NSArray *keys;
    unsigned i;
    
    if([viewTypes objectForKey:MIMEType]){
        return YES;
    }else{
        keys = [viewTypes allKeys];
        for(i=0; i<[keys count]; i++){
            if([[keys objectAtIndex:i] hasSuffix:@"/"] && [MIMEType hasPrefix:[keys objectAtIndex:i]]){
                if([viewTypes objectForKey:[keys objectAtIndex:i]]){
                    return YES;
                }
            }
        }
    }
    return NO;
}

@end
