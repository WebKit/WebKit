/*	
    IFContentHandler.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFContentHandler.h>

static NSString *imageDocumentTemplate = nil;
static NSString *pluginDocumentTemplate = nil;
static NSString *textDocumentTemplate = nil;

static BOOL imageDocumentLoaded = NO;
static BOOL pluginDocumentLoaded = NO;
static BOOL textDocumentLoaded = NO;

@implementation IFContentHandler

- initWithURL:(NSURL *)URL MIMEType:(NSString *)theMIMEType MIMEHandlerType:(IFMIMEHandlerType)theMIMEHandlerType;
{
    handlerType = theMIMEHandlerType;
    MIMEType = [theMIMEType retain];
    URLString = [[URL absoluteString] retain];
    
    return self;
}

- (NSString *) HTMLDocument
{
    NSString *path;
    NSBundle *bundle;
    NSData *data;
    
    if(handlerType == IFMIMEHANDLERTYPE_IMAGE){
        if(!imageDocumentLoaded){
            bundle = [NSBundle bundleForClass:[IFContentHandler class]];
            if ((path = [bundle pathForResource:@"image_document_template" ofType:@"html"])) {
                data = [[NSData alloc] initWithContentsOfFile:path];
                if (data) {
                    imageDocumentTemplate = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                    [data release];
                }
            }
            imageDocumentLoaded = YES;
        }
        return [NSString stringWithFormat:imageDocumentTemplate, URLString, URLString];
    }
    
    else if(handlerType == IFMIMEHANDLERTYPE_PLUGIN){
        if(!pluginDocumentLoaded){
            bundle = [NSBundle bundleForClass:[IFContentHandler class]];
            if ((path = [bundle pathForResource:@"plugin_document_template" ofType:@"html"])) {
                data = [[NSData alloc] initWithContentsOfFile:path];
                if (data) {
                    pluginDocumentTemplate = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                    [data release];
                }
            }    
            pluginDocumentLoaded = YES;        
        }
        return [NSString stringWithFormat:pluginDocumentTemplate, URLString, URLString, MIMEType, @"%", @"%"];    
    }

    else if(handlerType == IFMIMEHANDLERTYPE_TEXT){    
        if(!textDocumentLoaded){
            bundle = [NSBundle bundleForClass:[IFContentHandler class]];
            if ((path = [bundle pathForResource:@"text_document_template" ofType:@"html"])) {
                data = [[NSData alloc] initWithContentsOfFile:path];
                if (data) {
                    textDocumentTemplate = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
                    [data release];
                }
            }
            textDocumentLoaded = YES;        
        }
        return [NSString stringWithFormat:textDocumentTemplate, URLString];
    }
    return nil;
}

- (NSString *) textHTMLDocumentBottom;
{
    return @"</pre></body></html>";
}

-(void)dealloc
{
    [MIMEType release];
    [URLString release];
}
@end
