/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <Foundation/Foundation.h>

#ifdef __cplusplus

class KWQKHTMLPart;
class KHTMLView;
class RenderArena;

namespace khtml {
    class RenderPart;
    class RenderObject;
}

typedef khtml::RenderPart KHTMLRenderPart;

#else

@class KWQKHTMLPart;
@class KHTMLView;
@class KHTMLRenderPart;
@class RenderArena;

#endif

@class WebError;
@class WebFrame;

@protocol WebCoreDOMTreeCopier;
@protocol WebCoreRenderTreeCopier;
@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;
@protocol WebDOMDocument;
@protocol WebDOMNode;

extern NSString *WebCoreElementFrameKey;
extern NSString *WebCoreElementImageAltStringKey;
extern NSString *WebCoreElementImageKey;
extern NSString *WebCoreElementImageLocationKey;
extern NSString *WebCoreElementImageURLKey;
extern NSString *WebCoreElementIsSelectedTextKey;
extern NSString *WebCoreElementLinkURLKey;
extern NSString *WebCoreElementLinkTargetFrameKey;
extern NSString *WebCoreElementLinkLabelKey;
extern NSString *WebCoreElementLinkTitleKey;

// WebCoreBridge objects are used by WebCore to abstract away operations that need
// to be implemented by library clients, for example WebKit. The objects are also
// used in the opposite direction, for simple access to WebCore functions without dealing
// directly with the KHTML C++ classes.

// A WebCoreBridge creates and holds a reference to a KHTMLPart.

// The WebCoreBridge interface contains methods for use by the non-WebCore side of the bridge.

enum FrameBorderStyle {
    NoFrameBorder = 1,
    SunkenFrameBorder = 2,
    PlainFrameBorder = 4
};

@interface WebCoreBridge : NSObject
{
    KWQKHTMLPart *_part;
    KHTMLRenderPart *_renderPart;
    RenderArena *_renderPartArena;
}

- (void)setRenderPart:(KHTMLRenderPart *)renderPart;
- (KHTMLRenderPart *)renderPart;

- (void)setName:(NSString *)name;
- (NSString *)name;

- (KWQKHTMLPart *)part;

- (void)setParent:(WebCoreBridge *)parent;

- (void)openURL:(NSString *)URL reload:(BOOL)reload
    contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified
    pageCache:(NSDictionary *)pageCache;
- (void)addData:(NSData *)data withEncoding:(NSString *)encoding;
- (void)addData:(NSData *)data withOverrideEncoding:(NSString *)encoding;
- (void)closeURL;
- (void)saveDocumentState;
- (void)restoreDocumentState;

- (BOOL)canCachePage;
- (BOOL)saveDocumentToPageCache;

- (void)end;

- (NSString *)URL;
- (NSString *)referrer;

- (void)installInFrame:(NSView *)view;
- (void)removeFromFrame;

- (void)scrollToAnchor:(NSString *)anchor;
- (void)scrollToAnchorWithURL:(NSString *)URL;

- (void)createKHTMLViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh;

- (BOOL)isFrameSet;

- (void)reapplyStyles;
- (void)forceLayout;
- (BOOL)needsLayout;
- (void)adjustFrames:(NSRect)rect;
- (void)drawRect:(NSRect)rect;

- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseMoved:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;

// Resets the cursor to arrow.  This is useful to flush everyone's idea of the current cursor,
// so we aren't foiled by optimizations where a layer doesn't change the cursor because of its
// (incorrect) idea of what the cursor already is.
- (void)resetCursor;

- (NSView *)nextKeyView;
- (NSView *)previousKeyView;

- (NSView *)nextKeyViewInsideWebViews;
- (NSView *)previousKeyViewInsideWebViews;

- (NSObject *)copyDOMTree:(id <WebCoreDOMTreeCopier>)copier;
- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier;
- (NSString *)renderTreeAsExternalRepresentation;

- (NSDictionary *)elementAtPoint:(NSPoint)point;

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag;
- (void)jumpToSelection;

- (void)setTextSizeMultiplier:(float)multiplier;

- (CFStringEncoding)textEncoding;

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string;

- (id <WebDOMDocument>)DOMDocument;

- (void)setSelectionFrom:(id <WebDOMNode>)start startOffset:(int)startOffset to:(id <WebDOMNode>)end endOffset:(int) endOffset;

- (NSString *)selectedString;
- (NSAttributedString *)selectedAttributedString;

- (void)selectAll;
- (void)deselectAll;

- (id <WebDOMNode>)selectionStart;
- (int)selectionStartOffset;
- (id <WebDOMNode>)selectionEnd;
- (int)selectionEndOffset;

- (NSAttributedString *)attributedStringFrom:(id <WebDOMNode>)startNode startOffset:(int)startOffset to:(id <WebDOMNode>)endNode endOffset:(int)endOffset;

- (int)frameBorderStyle;

+ (NSString *)stringWithData:(NSData *)data textEncoding:(CFStringEncoding)textEncoding;
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName;

- (BOOL)interceptKeyEvent:(NSEvent *)event toView:(NSView *)view;

@end

// The WebCoreBridge protocol contains methods for use by the WebCore side of the bridge.

@protocol WebCoreBridge

- (NSArray *)childFrames; // WebCoreBridge objects
- (WebCoreBridge *)mainFrame;
- (WebCoreBridge *)findFramedNamed:(NSString *)name;
- (WebCoreBridge *)findOrCreateFramedNamed:(NSString *)name;
/* Creates a name for an frame unnamed in the HTML.  It should produce repeatable results for loads of the same frameset. */
- (NSString *)generateFrameName;

- (void)loadURL:(NSString *)URL reload:(BOOL)reload triggeringEvent:(NSEvent *)event isFormSubmission:(BOOL)isFormSubmission;
- (void)postWithURL:(NSString *)URL data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event;

- (WebCoreBridge *)createWindowWithURL:(NSString *)URL frameName:(NSString *)name;
- (void)showWindow;

- (NSString *)userAgentForURL:(NSString *)URL;

- (void)setTitle:(NSString *)title;
- (void)setStatusText:(NSString *)status;

- (void)setIconURL:(NSString *)URL;
- (void)setIconURL:(NSString *)URL withType:(NSString *)string;

- (WebCoreBridge *)createChildFrameNamed:(NSString *)frameName withURL:(NSString *)URL
    renderPart:(KHTMLRenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (BOOL)areToolbarsVisible;
- (void)setToolbarsVisible:(BOOL)visible;
- (BOOL)isStatusBarVisible;
- (void)setStatusBarVisible:(BOOL)visible;
- (BOOL)areScrollbarsVisible;
- (void)setScrollbarsVisible:(BOOL)visible;
- (NSWindow *)window;
- (void)setWindowFrame:(NSRect)frame;

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)loader withURL:(NSString *)URL;
- (void)objectLoadedFromCacheWithURL:(NSString *)URL response:(id)response size:(unsigned)bytes;
- (BOOL)isReloading;

- (void)reportClientRedirectToURL:(NSString *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory;
- (void)reportClientRedirectCancelled;

- (void)unfocusWindow;

- (NSView *)nextKeyViewOutsideWebViews;
- (NSView *)previousKeyViewOutsideWebViews;

- (BOOL)defersLoading;
- (void)setDefersLoading:(BOOL)loading;
- (void)saveDocumentState:(NSArray *)documentState;
- (NSArray *)documentState;

- (void)setNeedsReapplyStyles;
- (void)setNeedsLayout;

- (NSString *)requestedURL;

- (NSView *)viewForPluginWithURL:(NSString *)URL attributes:(NSArray *)attributesArray baseURL:(NSString *)baseURL MIMEType:(NSString *)MIMEType;
- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame attributes:(NSDictionary *)attributes baseURL:(NSString *)baseURL;

- (BOOL)saveDocumentToPageCache:(id)documentInfo;

- (int)getObjectCacheSize;

- (BOOL)frameRequiredForMIMEType: (NSString*)mimeType;

- (void)loadEmptyDocumentSynchronously;

- (NSString *)MIMETypeForPath:(NSString *)path;

- (void)handleMouseDragged:(NSEvent *)event;
- (void)handleAutoscrollForMouseDragged:(NSEvent *)event;
- (BOOL)mayStartDragWithMouseDragged:(NSEvent *)event;

@end

// This interface definition allows those who hold a WebCoreBridge * to call all the methods
// in the WebCoreBridge protocol without requiring the base implementation to supply the methods.
// This idiom is appropriate because WebCoreBridge is an abstract class.

@interface WebCoreBridge (SubclassResponsibility) <WebCoreBridge>
@end

@protocol WebCoreDOMTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name value:(NSString *)value source:(NSString *)source children:(NSArray *)children;
@end

@protocol WebCoreRenderTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name position:(NSPoint)p rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children;
@end
