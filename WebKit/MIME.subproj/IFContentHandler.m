/*	
    IFContentHandler.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFContentHandler.h>

static NSString *imageDocumentTemplate = nil;
static NSString *pluginDocumentTemplate = nil;
static NSString *textDocumentTemplate = nil;

@implementation IFContentHandler

- initWithURL:(NSURL *)URL MIMEType:(NSString *)theMIMEType MIMEHandlerType:(IFMIMEHandlerType)theMIMEHandlerType;
{
    [super init];
    
    handlerType = theMIMEHandlerType;
    MIMEType = [theMIMEType retain];
    URLString = [[URL absoluteString] retain];
    
    return self;
}

- (NSString *)useTemplate:(NSString *)templateName withGlobal:(NSString **)global
{
    NSBundle *bundle;
    NSString *path;
    NSData *data;
    
    if (!*global) {
        bundle = [NSBundle bundleForClass:[IFContentHandler class]];
        path = [bundle pathForResource:templateName ofType:@"html"];
        if (path) {
            data = [[NSData alloc] initWithContentsOfFile:path];
            if (data) {
                *global = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                [data release];
            }
        }
    }
    return [NSString stringWithFormat:*global, URLString, MIMEType];
}

- (NSString *) HTMLDocument
{
    switch (handlerType) {
    case IFMIMEHANDLERTYPE_IMAGE:
        return [self useTemplate:@"image_document_template" withGlobal:&imageDocumentTemplate];
    case IFMIMEHANDLERTYPE_PLUGIN:
        return [self useTemplate:@"plugin_document_template" withGlobal:&pluginDocumentTemplate];
    case IFMIMEHANDLERTYPE_TEXT:
        return [self useTemplate:@"text_document_template" withGlobal:&textDocumentTemplate];
    default:
        return nil;
    }
}

- (NSString *) textHTMLDocumentBottom;
{
    return @"</pre></body></html>";
}

-(void)dealloc
{
    [MIMEType release];
    [URLString release];
    [super dealloc];
}

@end
