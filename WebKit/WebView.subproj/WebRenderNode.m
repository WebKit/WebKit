//
//  IFRenderNode.m
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFRenderNode.h"

#import <WebKit/IFWebView.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <khtmlview.h>
#import <khtml_part.h>
#import <xml/dom_docimpl.h>
#import <rendering/render_frames.h>

@implementation IFRenderNode

- initWithRenderObject:(khtml::RenderObject *)node
{
    NSMutableArray *collectChildren;
    
    [super init];

    collectChildren = [NSMutableArray array];

    name = [[NSString stringWithCString:node->renderName()] retain];
    x = node->xPos();
    y = node->yPos();
    width = node->width();
    height = node->height();

    for (khtml::RenderObject *child = node->firstChild(); child; child = child->nextSibling()) {
        [collectChildren addObject:[[[IFRenderNode alloc] initWithRenderObject: child] autorelease]];
    }

    khtml::RenderPart *part = dynamic_cast<khtml::RenderPart *>(node);
    if (part) {
        NSView *view = part->widget()->getView();
        if ([view isKindOfClass:[NSScrollView class]]) {
            NSScrollView *scrollView = (NSScrollView *)view;
            view = [scrollView documentView];
        }
        if ([view isKindOfClass:[IFWebView class]]) {
            IFWebView *webView = (IFWebView *)view;
            [collectChildren addObject:[[[IFRenderNode alloc] initWithWebView:webView] autorelease]];
        }
    }
    
    children = [collectChildren copy];
    
    return self;
}

- initWithWebView:(IFWebView *)view
{
    return [self initWithRenderObject:[[view documentView] _widget]->part()->xmlDocImpl()->renderer()];
}

- (void)dealloc
{
    [name release];
    [children release];
    
    [super dealloc];
}

- (NSArray *)children
{
    return children;
}

- (NSString *)name
{
    return name;
}

- (NSString *)positionString
{
    return [NSString stringWithFormat:@"(%d, %d)", x, y];
}

- (NSString *)widthString
{
    return [NSString stringWithFormat:@"%d", width];
}

- (NSString *)heightString
{
    return [NSString stringWithFormat:@"%d", height];
}

@end
