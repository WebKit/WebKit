//
//  IFDOMNode.m
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFDOMNode.h"

#import <WebKit/IFWebView.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <khtmlview.h>
#import <khtml_part.h>
#import <xml/dom_docimpl.h>

@implementation IFDOMNode

- initWithDOMNode:(DOM::NodeImpl *)node
{
    NSMutableArray *collectChildren;
    
    [super init];

    collectChildren = [NSMutableArray array];

    name = [node->nodeName().string().getNSString() copy];
    value = [node->nodeValue().string().getNSString() copy];
    source = [node->recursive_toHTML(1).getNSString() copy];
   
    for (DOM::NodeImpl *child = node->firstChild(); child; child->nextSibling())
        [collectChildren addObject:[[[IFDOMNode alloc] initWithDOMNode: child] autorelease]];
    
    children = [collectChildren copy];
    
    return self;
}

- initWithWebView:(IFWebView *)view
{
    return [self initWithDOMNode:[[view documentView] _widget]->part()->xmlDocImpl()];
}

- (void)dealloc
{
    [name release];
    [value release];
    [source release];
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

- (NSString *)value
{
    return value;
}

- (NSString *)source
{
    return source;
}

@end
