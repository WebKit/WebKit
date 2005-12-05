/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebHTMLView.h>

#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBaseNetscapePluginViewInternal.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebDOMOperationsPrivate.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebHTMLViewInternal.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNSAttributedStringExtras.h>
#import <WebKit/WebNSEventExtras.h>
#import <WebKit/WebNSFileManagerExtras.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSPrintOperationExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebStringTruncator.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKitSystemInterface.h>

#import <AppKit/NSAccessibility.h>

// Included so usage of _NSSoftLinkingGetFrameworkFuncPtr will compile
#import <mach-o/dyld.h> 


// need to declare this because AppKit does not make it available as API or SPI
extern NSString *NSMarkedClauseSegmentAttributeName; 
extern NSString *NSTextInputReplacementRangeAttributeName; 

// Kill ring calls. Would be better to use NSKillRing.h, but that's not available in SPI.
void _NSInitializeKillRing(void);
void _NSAppendToKillRing(NSString *);
void _NSPrependToKillRing(NSString *);
NSString *_NSYankFromKillRing(void);
NSString *_NSYankPreviousFromKillRing(void);
void _NSNewKillRingSequence(void);
void _NSSetKillRingToYankedState(void);
void _NSResetKillRingOperationFlag(void);

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
- (void)_propagateDirtyRectsToOpaqueAncestors;
@end

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

@interface NSWindow (AppKitSecretsIKnowAbout)
- (id)_newFirstResponderAfterResigning;
@end

@interface NSAttributedString (AppKitSecretsIKnowAbout)
- (id)_initWithDOMRange:(DOMRange *)domRange;
- (DOMDocumentFragment *)_documentFromRange:(NSRange)range document:(DOMDocument *)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface NSSpellChecker (CurrentlyPrivateForTextView)
- (void)learnWord:(NSString *)word;
@end

// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
#define PrintingMinimumShrinkFactor     1.25

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
#define PrintingMaximumShrinkFactor     2.0

#define AUTOSCROLL_INTERVAL             0.1

#define DRAG_LABEL_BORDER_X             4.0
#define DRAG_LABEL_BORDER_Y             2.0
#define DRAG_LABEL_RADIUS               5.0
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP        120.0
#define MAX_DRAG_LABEL_WIDTH                    320.0

#define DRAG_LINK_LABEL_FONT_SIZE   11.0
#define DRAG_LINK_URL_FONT_SIZE   10.0

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

// FIXME: This constant is copied from AppKit's _NXSmartPaste constant.
#define WebSmartPastePboardType @"NeXT smart paste pasteboard type"

#define STANDARD_WEIGHT 5
#define MIN_BOLD_WEIGHT 9
#define STANDARD_BOLD_WEIGHT 10

typedef enum {
    deleteSelectionAction,
    deleteKeyAction,
    forwardDeleteKeyAction
} WebDeletionAction;

// if YES, do the standard NSView hit test (which can't give the right result when HTML overlaps a view)
static BOOL forceNSViewHitTest = NO;

// if YES, do the "top WebHTMLView" it test (which we'd like to do all the time but can't because of Java requirements [see bug 4349721])
static BOOL forceWebHTMLViewHitTest = NO;

// Used to avoid linking with ApplicationServices framework for _DCMDictionaryServiceWindowShow
void *_NSSoftLinkingGetFrameworkFuncPtr(NSString *inUmbrellaFrameworkName,
                                        NSString *inFrameworkName,
                                        const char *inFuncName,
                                        const struct mach_header **ioCachedFrameworkImageHeaderPtr);


@interface WebHTMLView (WebTextSizing) <_WebDocumentTextSizing>
@end

@interface WebHTMLView (WebHTMLViewFileInternal)
- (BOOL)_imageExistsAtPaths:(NSArray *)paths;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText chosePlainText:(BOOL *)chosePlainText;
- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action;
- (float)_calculatePrintHeight;
- (void)_updateTextSizeMultiplier;
- (DOMRange *)_selectedRange;
- (BOOL)_shouldDeleteRange:(DOMRange *)range;
- (void)_deleteRange:(DOMRange *)range 
            killRing:(BOOL)killRing 
             prepend:(BOOL)prepend 
       smartDeleteOK:(BOOL)smartDeleteOK
      deletionAction:(WebDeletionAction)deletionAction;
- (void)_deleteSelection;
- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard;
- (NSView *)_hitViewForEvent:(NSEvent *)event;
- (void)updateFocusState;
- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString;
@end

@interface WebHTMLView (WebForwardDeclaration) // FIXME: Put this in a normal category and stop doing the forward declaration trick.
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize;
@end

@interface WebHTMLView (WebNSTextInputSupport) <NSTextInput>
- (void)_updateSelectionForInputManager;
- (void)_insertText:(NSString *)text selectInsertedText:(BOOL)selectText;
@end

@interface WebHTMLView (WebEditingStyleSupport)
- (DOMCSSStyleDeclaration *)_emptyStyle;
- (NSString *)_colorAsString:(NSColor *)color;
@end

@interface NSView (WebHTMLViewFileInternal)
- (void)_web_setPrintingModeRecursive;
- (void)_web_clearPrintingModeRecursive;
- (void)_web_layoutIfNeededRecursive;
- (void)_web_layoutIfNeededRecursive:(NSRect)rect testDirtyRect:(bool)testDirtyRect;
@end

@interface NSMutableDictionary (WebHTMLViewFileInternal)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

// Handles the complete: text command
@interface WebTextCompleteController : NSObject
{
@private
    WebHTMLView *_view;
    NSWindow *_popupWindow;
    NSTableView *_tableView;
    NSArray *_completions;
    NSString *_originalString;
    int prefixLength;
}
- (id)initWithHTMLView:(WebHTMLView *)view;
- (void)doCompletion;
- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft;
- (BOOL)filterKeyDown:(NSEvent *)event;
- (void)_reflectSelection;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    ASSERT(autoscrollTimer == nil);
    ASSERT(autoscrollTriggerEvent == nil);
    
    [mouseDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    [compController release];
    [firstResponderAtMouseDownTime release];

    [super dealloc];
}

@end

@implementation WebHTMLView (WebHTMLViewFileInternal)

- (BOOL)_imageExistsAtPaths:(NSArray *)paths
{
    NSArray *imageMIMETypes = [[WebImageRendererFactory sharedFactory] supportedMIMETypes];
    NSEnumerator *enumerator = [paths objectEnumerator];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = WKGetMIMETypeForExtension([path pathExtension]);
        if ([imageMIMETypes containsObject:MIMEType]) {
            return YES;
        }
    }
    
    return NO;
}

- (DOMDocumentFragment *)_documentFragmentWithPaths:(NSArray *)paths
{
    DOMDocumentFragment *fragment;
    NSArray *imageMIMETypes = [[WebImageRendererFactory sharedFactory] supportedMIMETypes];
    NSEnumerator *enumerator = [paths objectEnumerator];
    WebDataSource *dataSource = [self _dataSource];
    NSMutableArray *domNodes = [[NSMutableArray alloc] init];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = WKGetMIMETypeForExtension([path pathExtension]);
        if ([imageMIMETypes containsObject:MIMEType]) {
            WebResource *resource = [[WebResource alloc] initWithData:[NSData dataWithContentsOfFile:path]
                                                                  URL:[NSURL fileURLWithPath:path]
                                                             MIMEType:MIMEType 
                                                     textEncodingName:nil
                                                            frameName:nil];
            if (resource) {
                [domNodes addObject:[dataSource _imageElementWithImageResource:resource]];
                [resource release];
            }
        } else {
            // Non-image file types
            NSString *url = [[[NSURL fileURLWithPath:path] _webkit_canonicalize] _web_userVisibleString];
            [domNodes addObject:[[[self _bridge] DOMDocument] createTextNode: url]];
        }
    }
    
    fragment = [[self _bridge] documentFragmentWithNodesAsParagraphs:domNodes]; 
    
    [domNodes release];
    
    return [fragment firstChild] != nil ? fragment : nil;
}

+ (NSArray *)_excludedElementsForAttributedStringConversion
{
    static NSArray *elements = nil;
    if (elements == nil) {
        elements = [[NSArray alloc] initWithObjects:
            // Omit style since we want style to be inline so the fragment can be easily inserted.
            @"style",
            // Omit xml so the result is not XHTML.
            @"xml", 
            // Omit tags that will get stripped when converted to a fragment anyway.
            @"doctype", @"html", @"head", @"body",
            // Omit deprecated tags.
            @"applet", @"basefont", @"center", @"dir", @"font", @"isindex", @"menu", @"s", @"strike", @"u",
            // Omit object so no file attachments are part of the fragment.
            @"object", nil];
    }
    return elements;
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText chosePlainText:(BOOL *)chosePlainText
{
    NSArray *types = [pasteboard types];
    *chosePlainText = NO;

    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        if (archive) {
            DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithArchive:archive];
            [archive release];
            if (fragment) {
                return fragment;
            }
        }
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        DOMDocumentFragment *fragment = [self _documentFragmentWithPaths:[pasteboard propertyListForType:NSFilenamesPboardType]];
        if (fragment != nil) {
            return fragment;
        }
    }
    
    NSURL *URL;
    
    if ([types containsObject:NSHTMLPboardType]) {
        NSString *HTMLString = [pasteboard stringForType:NSHTMLPboardType];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                HTMLString = [HTMLString substringFromIndex:range.location];
            }
        }
        if ([HTMLString length] != 0) {
            return [[self _bridge] documentFragmentWithMarkupString:HTMLString baseURLString:nil];
        }
    }
        
    NSAttributedString *string = nil;
    if ([types containsObject:NSRTFDPboardType]) {
        string = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    }
    if (string == nil && [types containsObject:NSRTFPboardType]) {
        string = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    }
    if (string != nil) {
        NSDictionary *documentAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:
            [[self class] _excludedElementsForAttributedStringConversion], NSExcludedElementsDocumentAttribute,
            self, @"WebResourceHandler", nil];
        NSArray *subresources;
        DOMDocumentFragment *fragment = [string _documentFromRange:NSMakeRange(0, [string length]) 
                                                          document:[[self _bridge] DOMDocument] 
                                                documentAttributes:documentAttributes
                                                      subresources:&subresources];
        [documentAttributes release];
        [string release];
        return fragment;
    }
    
    if ([types containsObject:NSTIFFPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSTIFFPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.tiff"]
                                                         MIMEType:@"image/tiff" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    
    if ([types containsObject:NSPICTPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSPICTPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.pict"]
                                                         MIMEType:@"image/pict" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }    
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        NSString *URLString = [URL _web_userVisibleString];
        if ([URLString length] > 0) {
            return [[self _bridge] documentFragmentWithText:URLString];
        }
    }
    
    if (allowPlainText && [types containsObject:NSStringPboardType]) {
        *chosePlainText = YES;
        return [[self _bridge] documentFragmentWithText:[pasteboard stringForType:NSStringPboardType]];
    }
    
    return nil;
}

- (WebResource *)resourceForData:(NSData *)data preferredFilename:(NSString *)name
{
    // This method is called by [NSAttributedString _documentFromRange::::] 
    // which uses the URL of the resource for the fragment that it returns.
    NSString *extension = [name pathExtension];
    NSString *MIMEType = nil;
    if ([extension length] != 0) {
        MIMEType = WKGetMIMETypeForExtension(extension);
    }
    // Only support image resources.
    if (MIMEType == nil || ![[[WebImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:MIMEType]) {
        return nil;
    }
    NSURL *URL = [NSURL _web_URLWithUserTypedString:[NSString stringWithFormat:@"/%@", name] relativeToURL:[NSURL _web_uniqueWebDataURL]];
    WebResource *resource = [[[WebResource alloc] initWithData:data
                                                           URL:URL
                                                      MIMEType:MIMEType 
                                              textEncodingName:nil
                                                     frameName:nil] autorelease];
    [[self _dataSource] addSubresource:resource];
    return resource;
}

- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    BOOL chosePlainText;
    DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard allowPlainText:allowPlainText chosePlainText:&chosePlainText];
    WebBridge *bridge = [self _bridge];
    if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:[self _selectedRange] givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithFragment:fragment selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard] matchStyle:chosePlainText];
    }
}

- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMNode *child = [fragment firstChild];
    if ([fragment lastChild] == child && [child isKindOfClass:[DOMCharacterData class]]) {
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:[(DOMCharacterData *)child data] replacingDOMRange:range givenAction:action];
    } else {
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertNode:fragment replacingDOMRange:range givenAction:action];
    }
}

- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMRange *selectedRange = [self _selectedRange];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:selectedRange givenAction:action];
}

// Calculate the vertical size of the view that fits on a single page
- (float)_calculatePrintHeight
{
    // Obtain the print info object for the current operation
    NSPrintInfo *pi = [[NSPrintOperation currentOperation] printInfo];
    
    // Calculate the page height in points
    NSSize paperSize = [pi paperSize];
    return paperSize.height - [pi topMargin] - [pi bottomMargin];
}

- (void)_updateTextSizeMultiplier
{
    [[self _bridge] setTextSizeMultiplier:[[self _webView] textSizeMultiplier]];    
}

- (DOMRange *)_selectedRange
{
    return [[self _bridge] selectedDOMRange];
}

- (BOOL)_shouldDeleteRange:(DOMRange *)range
{
    if (range == nil || [range collapsed])
        return NO;
    
    if (![[self _bridge] canDeleteRange:range])
        return NO;
        
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldDeleteDOMRange:range];
}

- (void)_deleteRange:(DOMRange *)range 
            killRing:(BOOL)killRing 
             prepend:(BOOL)prepend 
       smartDeleteOK:(BOOL)smartDeleteOK 
      deletionAction:(WebDeletionAction)deletionAction
{
    if (![self _shouldDeleteRange:range]) {
        return;
    }

    WebBridge *bridge = [self _bridge];
    BOOL smartDelete = smartDeleteOK ? [self _canSmartCopyOrDelete] : NO;

    BOOL startNewKillRingSequence = _private->startNewKillRingSequence;

    if (killRing) {
        if (startNewKillRingSequence) {
            _NSNewKillRingSequence();
        }
        NSString *string = [bridge stringForRange:range];
        if (prepend) {
            _NSPrependToKillRing(string);
        } else {
            _NSAppendToKillRing(string);
        }
        startNewKillRingSequence = NO;
    }

    switch (deletionAction) {
        case deleteSelectionAction:
            [bridge setSelectedDOMRange:range affinity:NSSelectionAffinityDownstream closeTyping:YES];
            [bridge deleteSelectionWithSmartDelete:smartDelete];
            break;
        case deleteKeyAction:
            [bridge setSelectedDOMRange:range affinity:NSSelectionAffinityDownstream closeTyping:NO];
            [bridge deleteKeyPressedWithSmartDelete:smartDelete];
            break;
        case forwardDeleteKeyAction:
            [bridge setSelectedDOMRange:range affinity:NSSelectionAffinityDownstream closeTyping:NO];
            [bridge forwardDeleteKeyPressedWithSmartDelete:smartDelete];
            break;
    }

    _private->startNewKillRingSequence = startNewKillRingSequence;
}

- (void)_deleteSelection
{
    [self _deleteRange:[self _selectedRange]
              killRing:YES 
               prepend:NO
         smartDeleteOK:YES
        deletionAction:deleteSelectionAction];
}

- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard
{
    return [[self _webView] smartInsertDeleteEnabled] && [[pasteboard types] containsObject:WebSmartPastePboardType];
}

- (NSView *)_hitViewForEvent:(NSEvent *)event
{
    // Usually, we hack AK's hitTest method to catch all events at the topmost WebHTMLView.  
    // Callers of this method, however, want to query the deepest view instead.
    forceNSViewHitTest = YES;
    NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
    forceNSViewHitTest = NO;    
    return hitView;
}

- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString
{
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [self _selectedArchive];
        [pasteboard setData:[archive data] forType:WebArchivePboardType];
    }
    
    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:NSRTFDPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }        
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }        
    if ([types containsObject:NSRTFPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }
        if ([attributedString containsAttachments]) {
            attributedString = [attributedString _web_attributedStringByStrippingAttachmentCharacters];
        }
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        NSMutableString *s = [[self selectedString] mutableCopy];
        const unichar NonBreakingSpaceCharacter = 0xA0;
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
    
    if ([self _canSmartCopyOrDelete] && [types containsObject:WebSmartPastePboardType]) {
        [pasteboard setData:nil forType:WebSmartPastePboardType];
    }
}

- (void)updateFocusState
{
    // This method does the job of updating the view based on the view's firstResponder-ness and
    // the window key-ness of the window containing this view. This involves four kinds of 
    // drawing updates right now, all handled in WebCore in response to the call over the bridge. 
    // 
    // The four display attributes are as follows:
    // 
    // 1. The background color used to draw behind selected content (active | inactive color)
    // 2. Caret blinking (blinks | does not blink)
    // 3. The drawing of a focus ring around links in web pages.
    // 4. Changing the tint of controls from clear to aqua/graphite and vice versa
    //
    // Also, this is responsible for letting the bridge know if the window has gained or lost focus
    // so we can send focus and blur events.
    
    WebBridge *bridge = [self _bridge];
    BOOL windowIsKey = [[self window] isKeyWindow];
    
    BOOL flag = !_private->resigningFirstResponder && windowIsKey && [self _web_firstResponderCausesFocusDisplay];
    [bridge setDisplaysWithFocusAttributes:flag];
    
    [bridge setWindowHasFocus:windowIsKey];
}

@end

@implementation WebHTMLView (WebPrivate)

- (void)_reset
{
    [WebImageRenderer stopAnimationsInView:self];
}

+ (NSArray *)supportedMIMETypes
{
    return [WebHTMLRepresentation supportedMIMETypes];
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WKMouseMovedNotification() object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

- (void)_frameOrBoundsChanged
{
    if (!NSEqualSizes(_private->lastLayoutSize, [(NSClipView *)[self superview] documentVisibleRect].size)) {
        [self setNeedsLayout:YES];
        [self setNeedsDisplay:YES];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }

    NSPoint origin = [[self superview] bounds].origin;
    if (!NSEqualPoints(_private->lastScrollPosition, origin)) {
        [[self _bridge] sendScrollEvent];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }
    _private->lastScrollPosition = origin;

    SEL selector = @selector(_updateMouseoverWithFakeEvent);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:selector object:nil];
    [self performSelector:selector withObject:nil afterDelay:0];
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
    _subviews = nil;
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

// This is called when we are about to draw, but before our dirty rect is propagated to our ancestors.
// That's the perfect time to do a layout, except that ideally we'd want to be sure that we're dirty
// before doing it. As a compromise, when we're opaque we do the layout only when actually asked to
// draw, but when we're transparent we do the layout at this stage so views behind us know that they
// need to be redrawn (in case the layout causes some things to get dirtied).
- (void)_propagateDirtyRectsToOpaqueAncestors
{
    if (![[self _webView] drawsBackground]) {
        [self _web_layoutIfNeededRecursive];
    }
    [super _propagateDirtyRectsToOpaqueAncestors];
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (wasInPrintingMode != isPrinting) {
        if (isPrinting) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }

    [self _web_layoutIfNeededRecursive: rect testDirtyRect:YES];
    [_subviews makeObjectsPerformSelector:@selector(_propagateDirtyRectsToOpaqueAncestors)];

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (wasInPrintingMode != isPrinting) {
            if (isPrinting) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        NSRect boundsBeforeLayout = [self bounds];
        [self _web_layoutIfNeededRecursive: visRect testDirtyRect:NO];

        // If layout changes the view's bounds, then we need to recompute the visRect.
        // That's because the visRect passed to us was based on the bounds at the time
        // we were called. This method is only displayed to draw "all", so it's safe
        // to just call visibleRect to compute the entire rectangle.
        if (!NSEqualRects(boundsBeforeLayout, [self bounds])) {
            visRect = [self visibleRect];
        }

        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _restoreSubviews];
    }
}

- (BOOL)_insideAnotherHTMLView
{
    NSView *view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WebHTMLView class]]) {
            return YES;
        }
    }
    return NO;
}

- (void)scrollPoint:(NSPoint)point
{
    // Since we can't subclass NSTextView to do what we want, we have to second guess it here.
    // If we get called during the handling of a key down event, we assume the call came from
    // NSTextView, and ignore it and use our own code to decide how to page up and page down
    // We are smarter about how far to scroll, and we have "superview scrolling" logic.
    NSEvent *event = [[self window] currentEvent];
    if ([event type] == NSKeyDown) {
        const unichar pageUp = NSPageUpFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageUp length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageUp:) with:nil];
            return;
        }
        const unichar pageDown = NSPageDownFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageDown length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageDown:) with:nil];
            return;
        }
    }
    
    [super scrollPoint:point];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all events for objects inside them.
    // To get those events, we prevent hit testing from AppKit.

    // But there are three exceptions to this:
    //   1) For right mouse clicks and control clicks we don't yet have an implementation
    //      that works for nested views, so we let the hit testing go through the
    //      standard NSView code path (needs to be fixed, see bug 4361618).
    //   2) Java depends on doing a hit test inside it's mouse moved handling,
    //      so we let the hit testing go through the standard NSView code path
    //      when the current event is a mouse move (except when we are calling
    //      from _updateMouseoverWithEvent, so we have to use a global,
    //      forceWebHTMLViewHitTest, for that)
    //   3) The acceptsFirstMouse: and shouldDelayWindowOrderingForEvent: methods
    //      both need to figure out which view to check with inside the WebHTMLView.
    //      They use a global to change the behavior of hitTest: so they can get the
    //      right view. The global is forceNSViewHitTest and the method they use to
    //      do the hit testing is _hitViewForEvent:. (But this does not work correctly
    //      when there is HTML overlapping the view, see bug 4361626)

    BOOL captureHitsOnSubviews;
    if (forceNSViewHitTest)
        captureHitsOnSubviews = NO;
    else if (forceWebHTMLViewHitTest)
        captureHitsOnSubviews = YES;
    else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = !([event type] == NSMouseMoved
            || [event type] == NSRightMouseDown
            || ([event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) != 0));
    }

    if (!captureHitsOnSubviews)
        return [super hitTest:point];
    if ([[self superview] mouse:point inRect:[self frame]])
        return self;
    return nil;
}

static WebHTMLView *lastHitView = nil;

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self) {
        lastHitView = nil;
    }
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    ASSERT(tag == TRACKING_RECT_TAG);
    if (_private != nil) {
        _private->trackingRectOwner = nil;
    }
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_private != nil) {
            _private->trackingRectOwner = nil;
        }
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    WebHTMLView *view = nil;
    forceWebHTMLViewHitTest = YES;
    NSView *hitView = [[[event window] contentView] hitTest:[event locationInWindow]];
    forceWebHTMLViewHitTest = NO;
    if ([hitView isKindOfClass:[WebHTMLView class]]) 
        view = (WebHTMLView *)hitView; 

    if (view)
        [view retain];

    if (lastHitView != view && lastHitView != nil) {
        // If we are moving out of a view (or frame), let's pretend the mouse moved
        // all the way out of that view. But we have to account for scrolling, because
        // khtml doesn't understand our clipping.
        NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
        float yScroll = visibleRect.origin.y;
        float xScroll = visibleRect.origin.x;

        event = [NSEvent mouseEventWithType:NSMouseMoved
                         location:NSMakePoint(-1 - xScroll, -1 - yScroll )
                         modifierFlags:[[NSApp currentEvent] modifierFlags]
                         timestamp:[NSDate timeIntervalSinceReferenceDate]
                         windowNumber:[[view window] windowNumber]
                         context:[[NSApp currentEvent] context]
                         eventNumber:0 clickCount:0 pressure:0];
        [[lastHitView _bridge] mouseMoved:event];
    }

    lastHitView = view;

    if (view) {
        [[view _bridge] mouseMoved:event];

        NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
        NSDictionary *element = [view elementAtPoint:point];

        // Have the web view send a message to the delegate so it can do status bar display.
        [[view _webView] _mouseDidMoveOverElement:element modifierFlags:[event modifierFlags]];

        // Set a tool tip; it won't show up right away but will if the user pauses.
        NSString *newToolTip = nil;
        if (_private->showsURLsInToolTips) {
            DOMHTMLElement *domElement = [element objectForKey:WebElementDOMNodeKey];
            if ([domElement isKindOfClass:[DOMHTMLInputElement class]]) {
                if ([[(DOMHTMLInputElement *)domElement type] isEqualToString:@"submit"])
                    newToolTip = [[(DOMHTMLInputElement *) domElement form] action];
            }
            if (newToolTip == nil)
                newToolTip = [[element objectForKey:WebCoreElementLinkURLKey] _web_userVisibleString];
        }
        if (newToolTip == nil)
            newToolTip = [element objectForKey:WebCoreElementTitleKey];
        [view _setToolTip:newToolTip];

        [view release];
    }
}

+ (NSArray *)_insertablePasteboardTypes
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:WebArchivePboardType, NSHTMLPboardType,
            NSFilenamesPboardType, NSTIFFPboardType, NSPICTPboardType, NSURLPboardType, 
            NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, NSColorPboardType, nil];
    }
    return types;
}

+ (NSArray *)_selectionPasteboardTypes
{
    // FIXME: We should put data for NSHTMLPboardType on the pasteboard but Microsoft Excel doesn't like our format of HTML (3640423).
    return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
}

- (WebArchive *)_selectedArchive
{
    NSArray *nodes;
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif
    NSString *markupString = [[self _bridge] markupStringFromRange:[self _selectedRange] nodes:&nodes];
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "copying markup took %f seconds.", duration);
#endif
    
    WebArchive *archive = [[self _dataSource] _archiveWithMarkupString:markupString nodes:nodes];

    if ([[self _bridge] isFrameSet]) {
        // Wrap the frameset document in an iframe so it can be pasted into
        // another document (which will have a body or frameset of its own). 
        NSString *iframeMarkup = [[NSString alloc] initWithFormat:@"<iframe frameborder=\"no\" marginwidth=\"0\" marginheight=\"0\" width=\"98%%\" height=\"98%%\" src=\"%@\"></iframe>", [[[self _dataSource] response] URL]];
        WebResource *iframeResource = [[WebResource alloc] initWithData:[iframeMarkup dataUsingEncoding:NSUTF8StringEncoding]
                                                                  URL:[NSURL URLWithString:@"about:blank"]
                                                             MIMEType:@"text/html"
                                                     textEncodingName:@"UTF-8"
                                                            frameName:nil];
        
        NSArray *subframeArchives = [NSArray arrayWithObject:archive];
        archive = [[[WebArchive alloc] initWithMainResource:iframeResource subresources:nil subframeArchives:subframeArchives] autorelease];
        
        [iframeResource release];
        [iframeMarkup release];
    }

    return archive;
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    ASSERT([self _hasSelection]);
    NSArray *types = [self pasteboardTypesForSelection];
    
    // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSMutableArray *mutableTypes = nil;
    if (![attributedString containsAttachments]) {
        mutableTypes = [types mutableCopy];
        [mutableTypes removeObject:NSRTFDPboardType];
        types = mutableTypes;
    }
    
    [pasteboard declareTypes:types owner:nil];
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:attributedString];
    [mutableTypes release];
}

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    
    if (!label) {
        drawURLString = NO;
        label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                   toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width = labelSize.width + DRAG_LABEL_BORDER_X * 2;
    imageSize.height = labelSize.height + DRAG_LABEL_BORDER_Y * 2;
    if (drawURLString) {
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
            imageSize.width = MAX(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
            clipURLString = YES;
        } else {
            imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
        }
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithCalibratedRed: 0.7 green: 0.7 blue: 0.7 alpha: 0.8] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0, imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20,DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path fill];
        
    NSColor *topColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.75];
    NSColor *bottomColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.5];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:urlFont];

        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
             withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (BOOL)_startDraggingImage:(NSImage *)wcDragImage at:(NSPoint)wcDragLoc operation:(NSDragOperation)op event:(NSEvent *)mouseDraggedEvent sourceIsDHTML:(BOOL)srcIsDHTML DHTMLWroteData:(BOOL)dhtmlWroteData
{
    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:mouseDownPoint];

    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    BOOL isSelected = [[element objectForKey:WebElementIsSelectedKey] boolValue];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;

    NSPoint mouseDraggedPoint = [self convertPoint:[mouseDraggedEvent locationInWindow] fromView:nil];
    _private->webCoreDragOp = op;     // will be DragNone if WebCore doesn't care
    NSImage *dragImage = nil;
    NSPoint dragLoc = { 0, 0 }; // quiet gcc 4.0 warning

    // We allow WebCore to override the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    // We could verify that ActionDHTML is allowed, although WebCore does claim to respect the action.
    if (wcDragImage) {
        dragImage = wcDragImage;
        // wcDragLoc is the cursor position relative to the lower-left corner of the image.
        // We add in the Y dimension because we are a flipped view, so adding moves the image down.
        if (linkURL) {
            // see HACK below
            dragLoc = NSMakePoint(mouseDraggedPoint.x - wcDragLoc.x, mouseDraggedPoint.y + wcDragLoc.y);
        } else {
            dragLoc = NSMakePoint(mouseDownPoint.x - wcDragLoc.x, mouseDownPoint.y + wcDragLoc.y);
        }
        _private->dragOffset = wcDragLoc;
    }
    
    WebView *webView = [self _webView];
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    WebImageRenderer *image = [element objectForKey:WebElementImageKey];
    BOOL startedDrag = YES;  // optimism - we almost always manage to start the drag

    // note per kwebster, the offset arg below is always ignored in positioning the image
    if (imageURL != nil && image != nil && (_private->dragSourceActionMask & WebDragSourceActionImage)) {
        id source = self;
        if (!dhtmlWroteData) {
            // Select the image when it is dragged. This allows the image to be moved via MoveSelectionCommandImpl and this matches NSTextView's behavior.
            DOMHTMLElement *imageElement = [element objectForKey:WebElementDOMNodeKey];
            ASSERT(imageElement != nil);
            [webView setSelectedDOMRange:[[[self _bridge] DOMDocument] _createRangeWithNode:imageElement] affinity:NSSelectionAffinityDownstream];
            _private->draggingImageURL = [imageURL retain];
            source = [pasteboard _web_declareAndWriteDragImage:image
                                                           URL:linkURL ? linkURL : imageURL
                                                         title:[element objectForKey:WebElementImageAltStringKey]
                                                       archive:[imageElement webArchive]
                                                        source:self];
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionImage fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            [self _web_dragImage:[element objectForKey:WebElementImageKey]
                            rect:[[element objectForKey:WebElementImageRectKey] rectValue]
                           event:_private->mouseDownEvent
                      pasteboard:pasteboard
                          source:source
                          offset:&_private->dragOffset];
        } else {
            [self dragImage:dragImage
                         at:dragLoc
                     offset:NSZeroSize
                      event:_private->mouseDownEvent
                 pasteboard:pasteboard
                     source:source
                  slideBack:YES];
        }
    } else if (linkURL && (_private->dragSourceActionMask & WebDragSourceActionLink)) {
        if (!dhtmlWroteData) {
            NSArray *types = [NSPasteboard _web_writableTypesForURL];
            [pasteboard declareTypes:types owner:self];
            [pasteboard _web_writeURL:linkURL andTitle:[element objectForKey:WebElementLinkLabelKey] types:types];            
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionLink fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [self _dragImageForLinkElement:element];
            NSSize offset = NSMakeSize([dragImage size].width / 2, -DRAG_LABEL_BORDER_Y);
            dragLoc = NSMakePoint(mouseDraggedPoint.x - offset.width, mouseDraggedPoint.y - offset.height);
            _private->dragOffset.x = offset.width;
            _private->dragOffset.y = -offset.height;        // inverted because we are flipped
        }
        // HACK:  We should pass the mouseDown event instead of the mouseDragged!  This hack gets rid of
        // a flash of the image at the mouseDown location when the drag starts.
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:mouseDraggedEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (isSelected && (_private->dragSourceActionMask & WebDragSourceActionSelection)) {
        if (!dhtmlWroteData) {
            [self _writeSelectionToPasteboard:pasteboard];
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionSelection fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [self _selectionDraggingImage];
            NSRect draggingRect = [self _selectionDraggingRect];
            dragLoc = NSMakePoint(NSMinX(draggingRect), NSMaxY(draggingRect));
            _private->dragOffset.x = mouseDownPoint.x - dragLoc.x;
            _private->dragOffset.y = dragLoc.y - mouseDownPoint.y;        // inverted because we are flipped
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (srcIsDHTML) {
        ASSERT(_private->dragSourceActionMask & WebDragSourceActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionDHTML fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            // WebCore should have given us an image, but we'll make one up
            NSString *imagePath = [[NSBundle bundleForClass:[self class]] pathForResource:@"missing_image" ofType:@"tiff"];
            dragImage = [[[NSImage alloc] initWithContentsOfFile:imagePath] autorelease];
            NSSize imageSize = [dragImage size];
            dragLoc = NSMakePoint(mouseDownPoint.x - imageSize.width/2, mouseDownPoint.y + imageSize.height/2);
            _private->dragOffset.x = imageSize.width/2;
            _private->dragOffset.y = imageSize.height/2;        // inverted because we are flipped
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else {
        // Only way I know if to get here is if the original element clicked on in the mousedown is no longer
        // under the mousedown point, so linkURL, imageURL and isSelected are all false/nil.
        startedDrag = NO;
    }
    return startedDrag;
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event
{
    [self autoscroll:event];
    [self _startAutoscrollTimer:event];
}

- (BOOL)_mayStartDragAtEventLocation:(NSPoint)location
{
    NSPoint mouseDownPoint = [self convertPoint:location fromView:nil];
    NSDictionary *mouseDownElement = [self elementAtPoint:mouseDownPoint];

    ASSERT([self _webView]);
    if ([mouseDownElement objectForKey: WebElementImageKey] != nil &&
        [mouseDownElement objectForKey: WebElementImageURLKey] != nil && 
        [[[self _webView] preferences] loadsImagesAutomatically] && 
        (_private->dragSourceActionMask & WebDragSourceActionImage)) {
        return YES;
    }
    
    if ([mouseDownElement objectForKey:WebElementLinkURLKey] != nil && 
        (_private->dragSourceActionMask & WebDragSourceActionLink)) {
        return YES;
    }
    
    if ([[mouseDownElement objectForKey:WebElementIsSelectedKey] boolValue] &&
        (_private->dragSourceActionMask & WebDragSourceActionSelection)) {
        return YES;
    }
    
    return NO;
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (void)_web_setPrintingModeRecursive
{
    [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_setPrintingModeRecursive];
}

- (void)_web_clearPrintingModeRecursive
{
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_clearPrintingModeRecursive];
}

- (void)_layoutIfNeeded
{
    ASSERT(!_private->subviewsSetAside);

    if ([[self _bridge] needsLayout])
        _private->needsLayout = YES;
    if (_private->needsToApplyStyles || _private->needsLayout)
        [self layout];
}

- (void)_web_layoutIfNeededRecursive
{
    [self _layoutIfNeeded];
    [super _web_layoutIfNeededRecursive];
}

- (void)_web_layoutIfNeededRecursive:(NSRect)displayRect testDirtyRect:(bool)testDirtyRect
{
    ASSERT(!_private->subviewsSetAside);
    displayRect = NSIntersectionRect(displayRect, [self bounds]);

    if (!testDirtyRect || [self needsDisplay]) {
        if (testDirtyRect) {
            NSRect dirtyRect = [self _dirtyRect];
            displayRect = NSIntersectionRect(displayRect, dirtyRect);
        }
        if (!NSIsEmptyRect(displayRect)) {
            [self _layoutIfNeeded];
        }
    }

    [super _web_layoutIfNeededRecursive:displayRect testDirtyRect:NO];
}

- (void)_startAutoscrollTimer: (NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
        _private->autoscrollTriggerEvent = [triggerEvent retain];
    }
}

// FIXME: _selectionRect is deprecated in favor of selectionRect, which is in protocol WebDocumentSelection.
// We can't remove this yet because it's still in use by Mail.
- (NSRect)_selectionRect
{
    return [self selectionRect];
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}

- (void)_autoscroll
{
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    BOOL isStillDown = WKMouseIsDown();   
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

- (BOOL)_canCopy
{
    // Copying can be done regardless of whether you can edit.
    return [self _hasSelection];
}

- (BOOL)_canCut
{
    return [self _hasSelection] && [self _isEditable];
}

- (BOOL)_canDelete
{
    return [self _hasSelection] && [self _isEditable];
}

- (BOOL)_canPaste
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_canEdit
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_hasSelection
{
    return [[self _bridge] selectionState] == WebSelectionStateRange;
}

- (BOOL)_hasSelectionOrInsertionPoint
{
    return [[self _bridge] selectionState] != WebSelectionStateNone;
}

- (BOOL)_isEditable
{
    return [[self _webView] isEditable] || [[self _bridge] isSelectionEditable];
}

- (BOOL)_isSelectionMisspelled
{
    NSString *selectedString = [self selectedString];
    unsigned length = [selectedString length];
    if (length == 0) {
        return NO;
    }
    NSRange range = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:selectedString
                                                                    startingAt:0
                                                                      language:nil
                                                                          wrap:NO
                                                        inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]
                                                                     wordCount:NULL];
    return range.length == length;
}

- (NSArray *)_guessesForMisspelledSelection
{
    ASSERT([[self selectedString] length] != 0);
    return [[NSSpellChecker sharedSpellChecker] guessesForWord:[self selectedString]];
}

- (void)_changeSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    if ([self _shouldReplaceSelectionWithText:[sender title] givenAction:WebViewInsertActionPasted]) {
        [[self _bridge] replaceSelectionWithText:[sender title] selectReplacement:YES smartReplace:NO];
    }
}

- (void)_ignoreSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    [[NSSpellChecker sharedSpellChecker] ignoreWord:[self selectedString] inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]];
}

- (void)_learnSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    [[NSSpellChecker sharedSpellChecker] learnWord:[self selectedString]];
}

- (void)_lookUpInDictionaryFromMenu:(id)sender
{
    // This should only be called when there's a selection, but play it safe.
    if (![self _hasSelection]) {
        return;
    }
    
    // Soft link to dictionary-display function to avoid linking another framework (ApplicationServices/LangAnalysis)
    static OSStatus (*__dictionaryServiceWindowShow)(id inWordString, NSRect inWordBoundary, UInt16 inLineDirection) = NULL;
    static const struct mach_header *frameworkImageHeader = NULL;
    static BOOL lookedForFunction = NO;
    if (!lookedForFunction) {
        __dictionaryServiceWindowShow = _NSSoftLinkingGetFrameworkFuncPtr(@"ApplicationServices", @"LangAnalysis", "_DCMDictionaryServiceWindowShow", &frameworkImageHeader);
        lookedForFunction = YES;
    }
    if (!__dictionaryServiceWindowShow) {
        ERROR("Couldn't find _DCMDictionaryServiceWindowShow"); 
        return;
    }
    
    // FIXME: must check for right-to-left here
    NSWritingDirection writingDirection = NSWritingDirectionLeftToRight;
    
    NSAttributedString *attrString = [self selectedAttributedString];
    // FIXME: the dictionary API expects the rect for the first line of selection. Passing
    // the rect for the entire selection, as we do here, positions the pop-up window near
    // the bottom of the selection rather than at the selected word.
    NSRect rect = [self convertRect:[[self _bridge] visibleSelectionRect] toView:nil];
    rect.origin = [[self window] convertBaseToScreen:rect.origin];
    NSData *data = [attrString RTFFromRange:NSMakeRange(0, [attrString length]) documentAttributes:nil];
    (void)__dictionaryServiceWindowShow(data, rect, (writingDirection == NSWritingDirectionRightToLeft) ? 1 : 0);
}

- (BOOL)_transparentBackground
{
    return _private->transparentBackground;
}

- (void)_setTransparentBackground:(BOOL)f
{
    _private->transparentBackground = f;
}

- (NSImage *)_selectionDraggingImage
{
    if ([self _hasSelection]) {
        NSImage *dragImage = [[self _bridge] selectionImage];
        [dragImage _web_dissolveToFraction:WebDragImageAlpha];
        return dragImage;
    }
    return nil;
}

- (NSRect)_selectionDraggingRect
{
    if ([self _hasSelection]) {
        return [[self _bridge] visibleSelectionRect];
    }
    return NSZeroRect;
}

@end

@implementation NSView (WebHTMLViewFileInternal)

- (void)_web_setPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_setPrintingModeRecursive)];
}

- (void)_web_clearPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_clearPrintingModeRecursive)];
}

- (void)_web_layoutIfNeededRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_layoutIfNeededRecursive)];
}

- (void)_web_layoutIfNeededRecursive: (NSRect)rect testDirtyRect:(bool)testDirtyRect
{
    unsigned index, count;
    for (index = 0, count = [_subviews count]; index < count; index++) {
        NSView *subview = [_subviews objectAtIndex:index];
        NSRect dirtiedSubviewRect = [subview convertRect: rect fromView: self];
        [subview _web_layoutIfNeededRecursive: dirtiedSubviewRect testDirtyRect:testDirtyRect];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewFileInternal)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewFileInternal)
@end

@implementation NSToolTipPanel (WebHTMLViewFileInternal)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@implementation WebHTMLView

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] 
                             returnTypes:[[self class] _insertablePasteboardTypes]];
    _NSInitializeKillRing();
}

- (void)_resetCachedWebPreferences:(NSNotification *)ignored
{
    WebPreferences *preferences = [[self _webView] preferences];
    // Check for nil because we might not yet have an associated webView when this is called
    if (preferences == nil) {
        preferences = [WebPreferences standardPreferences];
    }
    _private->showsURLsInToolTips = [preferences showsURLsInToolTips];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    // Make all drawing go through us instead of subviews.
    if (NSAppKitVersionNumber >= 711) {
        [self _setDrawsOwnDescendants:YES];
    }
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithDocumentView:self];
    _private->needsLayout = YES;
    [self _resetCachedWebPreferences:nil];
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_resetCachedWebPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
    
    return self;
}

- (void)dealloc
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private->pluginController destroyAllPlugins];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (void)finalize
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private->pluginController destroyAllPlugins];
    _private = nil;
    [super finalize];
}

- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self _hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (NSArray *)pasteboardTypesForSelection
{
    if ([self _canSmartCopyOrDelete]) {
        NSMutableArray *types = [[[[self class] _selectionPasteboardTypes] mutableCopy] autorelease];
        [types addObject:WebSmartPastePboardType];
        return types;
    } else {
        return [[self class] _selectionPasteboardTypes];
    }
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:nil];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [pasteboard declareTypes:types owner:nil];
    [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    return YES;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard
{
    [self _pasteWithPasteboard:pasteboard allowPlainText:YES];
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType != nil && [[self pasteboardTypesForSelection] containsObject:sendType] && [self _hasSelection]) {
        return self;
    } else if (returnType != nil && [[[self class] _insertablePasteboardTypes] containsObject:returnType] && [self _isEditable]) {
        return self;
    }
    return [[self nextResponder] validRequestorForSendType:sendType returnType:returnType];
}

- (void)selectAll:(id)sender
{
    [self selectAll];
}

// jumpToSelection is the old name for what AppKit now calls centerSelectionInVisibleArea. Safari
// was using the old jumpToSelection selector in its menu. Newer versions of Safari will us the
// selector centerSelectionInVisibleArea. We'll leave this old selector in place for two reasons:
// (1) compatibility between older Safari and newer WebKit; (2) other WebKit-based applications
// might be using the jumpToSelection: selector, and we don't want to break them.
- (void)jumpToSelection:(id)sender
{
    [self centerSelectionInVisibleArea:sender];
}

- (NSRect)selectionRect
{
    return [[self _bridge] selectionRect];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    WebBridge *bridge = [self _bridge];

    if (action == @selector(alignCenter:)
            || action == @selector(alignLeft:)
            || action == @selector(alignJustified:)
            || action == @selector(alignRight:)
            || action == @selector(changeAttributes:)
            || action == @selector(changeBaseWritingDirection:) // FIXME: check menu item based on writing direction
            || action == @selector(changeColor:)
            || action == @selector(changeFont:)
            || action == @selector(changeSpelling:)
            || action == @selector(_changeSpellingFromMenu:)
            || action == @selector(checkSpelling:)
            || action == @selector(complete:)
            || action == @selector(deleteBackward:)
            || action == @selector(deleteBackwardByDecomposingPreviousCharacter:)
            || action == @selector(deleteForward:)
            || action == @selector(deleteToBeginningOfLine:)
            || action == @selector(deleteToBeginningOfParagraph:)
            || action == @selector(deleteToEndOfLine:)
            || action == @selector(deleteToEndOfParagraph:)
            || action == @selector(deleteToMark:)
            || action == @selector(deleteWordBackward:)
            || action == @selector(deleteWordForward:)
            || action == @selector(insertBacktab:)
            || action == @selector(insertLineBreak:)
            || action == @selector(insertNewline:)
            || action == @selector(insertNewlineIgnoringFieldEditor:)
            || action == @selector(insertParagraphSeparator:)
            || action == @selector(insertTab:)
            || action == @selector(insertTabIgnoringFieldEditor:)
            || action == @selector(moveBackward:)
            || action == @selector(moveBackwardAndModifySelection:)
            || action == @selector(moveDown:)
            || action == @selector(moveDownAndModifySelection:)
            || action == @selector(moveForward:)
            || action == @selector(moveForwardAndModifySelection:)
            || action == @selector(moveLeft:)
            || action == @selector(moveLeftAndModifySelection:)
            || action == @selector(moveParagraphBackwardAndModifySelection:)
            || action == @selector(moveParagraphForwardAndModifySelection:)
            || action == @selector(moveRight:)
            || action == @selector(moveRightAndModifySelection:)
            || action == @selector(moveToBeginningOfDocument:)
            || action == @selector(moveToBeginningOfDocumentAndModifySelection:)
            || action == @selector(moveToBeginningOfLine:)
            || action == @selector(moveToBeginningOfLineAndModifySelection:)
            || action == @selector(moveToBeginningOfParagraph:)
            || action == @selector(moveToBeginningOfParagraphAndModifySelection:)
            || action == @selector(moveToEndOfDocument:)
            || action == @selector(moveToEndOfDocumentAndModifySelection:)
            || action == @selector(moveToEndOfLine:)
            || action == @selector(moveToEndOfLineAndModifySelection:)
            || action == @selector(moveToEndOfParagraph:)
            || action == @selector(moveToEndOfParagraphAndModifySelection:)
            || action == @selector(moveUp:)
            || action == @selector(moveUpAndModifySelection:)
            || action == @selector(moveWordBackward:)
            || action == @selector(moveWordBackwardAndModifySelection:)
            || action == @selector(moveWordForward:)
            || action == @selector(moveWordForwardAndModifySelection:)
            || action == @selector(moveWordLeft:)
            || action == @selector(moveWordLeftAndModifySelection:)
            || action == @selector(moveWordRight:)
            || action == @selector(moveWordRightAndModifySelection:)
            || action == @selector(pageDown:)
            || action == @selector(pageDownAndModifySelection:)
            || action == @selector(pageUp:)
            || action == @selector(pageUpAndModifySelection:)
            || action == @selector(pasteFont:)
            || action == @selector(showGuessPanel:)
            || action == @selector(toggleBaseWritingDirection:)
            || action == @selector(transpose:)
            || action == @selector(yank:)
            || action == @selector(yankAndSelect:)) {
        return [self _canEdit];
    } else if (action == @selector(capitalizeWord:)
               || action == @selector(lowercaseWord:)
               || action == @selector(uppercaseWord:)) {
        return [self _hasSelection] && [self _isEditable];
    } else if (action == @selector(centerSelectionInVisibleArea:)
            || action == @selector(copyFont:)
            || action == @selector(setMark:)) {
        return [self _hasSelectionOrInsertionPoint];
    } else if (action == @selector(changeDocumentBackgroundColor:)) {
        return [[self _webView] isEditable];
    } else if (action == @selector(copy:)) {
        return [bridge mayDHTMLCopy] || [self _canCopy];
    } else if (action == @selector(cut:)) {
        return [bridge mayDHTMLCut] || [self _canDelete];
    } else if (action == @selector(delete:)) {
        return [self _canDelete];
    } else if (action == @selector(_ignoreSpellingFromMenu:)
            || action == @selector(centerSelectionInVisibleArea:)
            || action == @selector(jumpToSelection:)
            || action == @selector(_learnSpellingFromMenu:)
            || action == @selector(takeFindStringFromSelection:)) {
        return [self _hasSelection];
    } else if (action == @selector(paste:) || action == @selector(pasteAsPlainText:) || action == @selector(pasteAsRichText:)) {
        return [bridge mayDHTMLPaste] || [self _canPaste];
    } else if (action == @selector(performFindPanelAction:)) {
        // FIXME: Not yet implemented.
        return NO;
    } else if (action == @selector(selectToMark:)
            || action == @selector(swapWithMark:)) {
        return [self _hasSelectionOrInsertionPoint] && [[self _bridge] markDOMRange] != nil;
    } else if (action == @selector(subscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"sub"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEdit];
    } else if (action == @selector(superscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"super"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEdit];
    } else if (action == @selector(underline:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setProperty:@"-khtml-text-decorations-in-effect" :@"underline" :@""];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEdit];
    } else if (action == @selector(unscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"baseline"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEdit];
    } else if (action == @selector(_lookUpInDictionaryFromMenu:)) {
        return [self _hasSelection];
    }
    
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown
            && !_private->handlingMouseDownEvent
            && NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil])) {
        return NO;
    }
    return YES;
}

- (BOOL)maintainsInactiveSelection
{
    // This method helps to determing whether the view should maintain
    // an inactive selection when the view is not first responder.
    // Traditionally, these views have not maintained such selections,
    // clearing them when the view was not first responder. However,
    // to fix bugs like this one:
    // <rdar://problem/3672088>: "Editable WebViews should maintain a selection even 
    //                            when they're not firstResponder"
    // it was decided to add a switch to act more like an NSTextView.
    // For now, however, the view only acts in this way when the
    // web view is set to be editable. This will maintain traditional
    // behavior for WebKit clients dating back to before this change,
    // and will likely be a decent switch for the long term, since
    // clients to ste the web view to be editable probably want it
    // to act like a "regular" Cocoa view in terms of its selection
    // behavior.
    id nextResponder = [[self window] _newFirstResponderAfterResigning];

    // Predict the case where we are losing first responder status only to
    // gain it back again.  Want to keep the selection in that case.
    if ([nextResponder isKindOfClass:[NSScrollView class]]) {
        id contentView = [nextResponder contentView];
        if (contentView) {
            nextResponder = contentView;
        }
    }
    if ([nextResponder isKindOfClass:[NSClipView class]]) {
        id documentView = [nextResponder documentView];
        if (documentView) {
            nextResponder = documentView;
        }
    }

    if (nextResponder == self)
        return YES;

    if (![[self _webView] maintainsInactiveSelection])
        return NO;
    
    // editable views lose selection when losing first responder status
    // to a widget in the same page, but not otherwise
    BOOL loseSelection = [nextResponder isKindOfClass:[NSView class]]
        && [nextResponder isDescendantOf:[self _webView]];

    return !loseSelection;
}

- (void)addMouseMovedObserver
{
    // Always add a mouse move observer if the DB requested, or if we're the key window.
    if (([[self window] isKeyWindow] && ![self _insideAnotherHTMLView]) ||
        [[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows]){
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
            name:WKMouseMovedNotification() object:nil];
        [self _frameOrBoundsChanged];
    }
}

- (void)removeMouseMovedObserver
{
    // Don't remove the observer if we're running the DB
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows])
        return;
        
    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:WKMouseMovedNotification() object:nil];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because our "layout" has to change
    // to extend the background the full height of the space and because some elements have
    // sizes that are based on the total size of the view.
    
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)removeSuperviewObservers
{
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    // Do this here in case the text size multiplier changed when a non-HTML
    // view was installed.
    if ([self superview] != nil) {
        [self _updateTextSizeMultiplier];
        [self addSuperviewObservers];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private){
        // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
        [self removeMouseMovedObserver];
        [self removeWindowObservers];
        [self removeSuperviewObservers];
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];
    
        [[self _pluginController] stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private) {
        [self _stopAutoscrollTimer];
        if ([self window]) {
            _private->lastScrollPosition = [[self superview] bounds].origin;
            [self addWindowObservers];
            [self addSuperviewObservers];
            [self addMouseMovedObserver];

            // Schedule this update, rather than making the call right now.
            // The reason is that placing the caret in the just-installed view requires
            // the HTML/XML document to be available on the WebCore side, but it is not
            // at the time this code is running. However, it will be there on the next
            // crank of the run loop. Doing this helps to make a blinking caret appear 
            // in a new, empty window "automatic".
            [self performSelector:@selector(updateFocusState) withObject:nil afterDelay:0];

            [[self _pluginController] startAllPlugins];
    
            _private->lastScrollPosition = NSZeroPoint;
            
            _private->inWindow = YES;
        } else {
            // Reset when we are moved out of a window after being moved into one.
            // Without this check, we reset ourselves before we even start.
            // This is only needed because viewDidMoveToWindow is called even when
            // the window is not changing (bug in AppKit).
            if (_private->inWindow) {
                [self _reset];
                _private->inWindow = NO;
            }
        }
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([WebPluginController isPlugInView:view]) {
        [[self _pluginController] addPlugin:view];
    }
}

- (void)willRemoveSubview:(NSView *)subview
{
    if ([WebPluginController isPlugInView:subview])
        [[self _pluginController] destroyPlugin:subview];
    [super willRemoveSubview:subview];
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStylesForDeviceType:
        _private->printing ? WebCoreDevicePrinter : WebCoreDeviceScreen];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustViewSize
{
    [self reapplyStyles];
    
    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    WKSetNSWindowShouldPostEventNotifications([self window], YES);

    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    if (minPageWidth > 0.0) {
        [[self _bridge] forceLayoutWithMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
    } else {
        [[self _bridge] forceLayoutAdjustingViewSize:adjustViewSize];
    }
    _private->needsLayout = NO;
    
    if (!_private->printing) {
        // get size of the containing dynamic scrollview, so
        // appearance and disappearance of scrollbars will not show up
        // as a size change
        NSSize newLayoutFrameSize = [[[self superview] superview] frame].size;
        if (_private->laidOutAtLeastOnce && !NSEqualSizes(_private->lastLayoutFrameSize, newLayoutFrameSize)) {
            [[self _bridge] sendResizeEvent];
        }
        _private->laidOutAtLeastOnce = YES;
        _private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
        _private->lastLayoutFrameSize = newLayoutFrameSize;
    }

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0.0 maximumPageWidth:0.0 adjustingViewSize:NO];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    [_private->compController endRevertingChange:NO moveLeft:NO];

    if ([[self _bridge] sendContextMenuEvent:event]) {
        return nil;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point];
    return [[self _webView] _menuForElement:element defaultItems:nil];
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return [[self _bridge] searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag];
}

- (DOMRange *)_documentRange
{
    return [[[self _bridge] DOMDocument] _documentRange];
}

- (NSString *)string
{
    return [[self _bridge] stringForRange:[self _documentRange]];
}

- (NSAttributedString *)_attributeStringFromDOMRange:(DOMRange *)range
{
    NSAttributedString *attributedString;
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif    
    attributedString = [[[NSAttributedString alloc] _initWithDOMRange:range] autorelease];
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "creating attributed string from selection took %f seconds.", duration);
#endif
    return attributedString;
}

- (NSAttributedString *)attributedString
{
    WebBridge *bridge = [self _bridge];
    DOMDocument *document = [bridge DOMDocument];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[document _documentRange]];
    if (attributedString == nil) {
        attributedString = [bridge attributedStringFrom:document startOffset:0 to:nil endOffset:0];
    }
    return attributedString;
}

- (NSString *)selectedString
{
    return [[self _bridge] selectedString];
}

- (NSAttributedString *)selectedAttributedString
{
    WebBridge *bridge = [self _bridge];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[self _selectedRange]];
    if (attributedString == nil) {
        attributedString = [bridge selectedAttributedString];
    }
    return attributedString;
}

- (void)selectAll
{
    [[self _bridge] selectAll];
}

// Remove the selection.
- (void)deselectAll
{
    [[self _bridge] deselectAll];
}

- (void)deselectText
{
    [[self _bridge] deselectText];
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    [super setNeedsDisplay: flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsToApplyStyles = flag;
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);

    // Work around AppKit bug <rdar://problem/3875305> rect passed to drawRect: is too large.
    // Ignore the passed-in rect and instead union in the rectangles from getRectsBeingDrawn.
    // This does a better job of clipping out rects that are entirely outside the visible area.
    const NSRect *rects;
    int count;
    [self getRectsBeingDrawn:&rects count:&count];

    // If count == 0 here, use the rect passed in for drawing. This is a workaround for:
    // <rdar://problem/3908282> REGRESSION (Mail): No drag image dragging selected text in Blot and Mail
    // The reason for the workaround is that this method is called explicitly from the code
    // to generate a drag image, and at that time, getRectsBeingDrawn:count: will return a zero count.
    if (count > 0) {
        rect = NSZeroRect;
        int i;
        for (i = 0; i < count; ++i) {
            rect = NSUnionRect(rect, rects[i]);
        }
        if (rect.size.height == 0 || rect.size.width == 0) {
            return;
        }
    }

    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside) {
        [self _restoreSubviews];
    }

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    NS_DURING {
        if ([self _transparentBackground]) {
            [[NSColor clearColor] set];
            NSRectFill (rect);
        }
        
        [[self _bridge] drawRect:rect];

        [(WebClipView *)[self superview] resetAdditionalClip];

        [NSGraphicsContext restoreGraphicsState];
    } NS_HANDLER {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    } NS_ENDHANDLER

#ifdef DEBUG_LAYOUT
    NSRect vframe = [self frame];
    [[NSColor blackColor] set];
    NSBezierPath *path;
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, 0)];
    [path lineToPoint:NSMakePoint(vframe.size.width, vframe.size.height)];
    [path closePath];
    [path stroke];
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, vframe.size.height)];
    [path lineToPoint:NSMakePoint(vframe.size.width, 0)];
    [path closePath];
    [path stroke];
#endif

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (subviewsWereSetAside) {
        [self _setAsideSubviews];
    }
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self addMouseMovedObserver];
    [self updateFocusState];
}

- (void)windowDidResignKey: (NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [_private->compController endRevertingChange:NO moveLeft:NO];
    [self removeMouseMovedObserver];
    [self updateFocusState];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [_private->compController endRevertingChange:NO moveLeft:NO];
    [[self _pluginController] destroyAllPlugins];
}

- (void)scrollWheel:(NSEvent *)event
{
    [self retain];
    
    if (![[self _bridge] sendScrollWheelEvent:event]) {
        [[self nextResponder] scrollWheel:event];
    }    
    
    [self release];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (void)_setMouseDownEvent:(NSEvent *)event
{
    ASSERT([event type] == NSLeftMouseDown || [event type] == NSRightMouseDown || [event type] == NSOtherMouseDown);

    if (event == _private->mouseDownEvent) {
        return;
    }

    [event retain];
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = event;

    [_private->firstResponderAtMouseDownTime release];
    _private->firstResponderAtMouseDownTime = [[[self window] firstResponder] retain];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    [hitHTMLView _setMouseDownEvent:event];
    
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse])
        return YES;
    
    if (hitHTMLView != nil) {
        [[hitHTMLView _bridge] setActivationEventNumber:[event eventNumber]];
        return [hitHTMLView _isSelectionEvent:event] ? [[hitHTMLView _bridge] eventMayStartDrag:event] : NO;
    } else {
        return [hitView acceptsFirstMouse:event];
    }
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    if (hitHTMLView != nil) {
        [hitHTMLView _setMouseDownEvent:event];
        return [hitHTMLView _isSelectionEvent:event] ? [[hitHTMLView _bridge] eventMayStartDrag:event] : NO;
    } else {
        return [hitView shouldDelayWindowOrderingForEvent:event];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    [self retain];

    _private->handlingMouseDownEvent = YES;

    // Record the mouse down position so we can determine drag hysteresis.
    [self _setMouseDownEvent:event];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        goto done;

    [_private->compController endRevertingChange:NO moveLeft:NO];

    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if (!([event modifierFlags] & NSControlKeyMask)) {
        _private->ignoringMouseDraggedEvents = NO;

        // Don't do any mouseover while the mouse is down.
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

        // Let KHTML get a chance to deal with the event. This will call back to us
        // to start the autoscroll timer if appropriate.
        [[self _bridge] mouseDown:event];
    }

done:
    [_private->firstResponderAtMouseDownTime release];
    _private->firstResponderAtMouseDownTime = nil;

    _private->handlingMouseDownEvent = NO;
    
    [self release];
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{   
    [self _stopAutoscrollTimer];
    
    _private->initiatedDrag = YES;
    [[self _webView] _setInitiatedDrag:YES];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    if (!_private->ignoringMouseDraggedEvents)
        [[self _bridge] mouseDragged:event];

    [self release];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    if (_private->webCoreDragOp == NSDragOperationNone) {
        return (NSDragOperationGeneric | NSDragOperationCopy);
    } else {
        return _private->webCoreDragOp;
    }
}

- (void)draggedImage:(NSImage *)image movedTo:(NSPoint)screenLoc
{
    NSPoint windowImageLoc = [[self window] convertScreenToBase:screenLoc];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceMovedTo:windowMouseLoc];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceEndedAt:windowMouseLoc operation:operation];

    _private->initiatedDrag = NO;
    [[self _webView] _setInitiatedDrag:NO];
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // khtml expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:windowMouseLoc
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent]; // This will also update the mouseover state.
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    ASSERT(_private->draggingImageURL);
    
    NSFileWrapper *wrapper = [[self _dataSource] _fileWrapperForURL:_private->draggingImageURL];
    if (wrapper == nil) {
        ERROR("Failed to create image file. Did the source image change while dragging? (<rdar://problem/4244861>)");
        return nil;
    }
    
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES]) {
        ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:]");
    }
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (BOOL)_canProcessDragWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    NSPasteboard *pasteboard = [draggingInfo draggingPasteboard];
    NSMutableSet *types = [NSMutableSet setWithArray:[pasteboard types]];
    [types intersectSet:[NSSet setWithArray:[WebHTMLView _insertablePasteboardTypes]]];
    if ([types count] == 0) {
        return NO;
    } else if ([types count] == 1 && 
               [types containsObject:NSFilenamesPboardType] && 
               ![self _imageExistsAtPaths:[pasteboard propertyListForType:NSFilenamesPboardType]]) {
        return NO;
    }
    
    NSPoint point = [self convertPoint:[draggingInfo draggingLocation] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point];
    if ([[self _webView] isEditable] || [[element objectForKey:WebElementDOMNodeKey] isContentEditable]) {
        if (_private->initiatedDrag && [[element objectForKey:WebElementIsSelectedKey] boolValue]) {
            // Can't drag onto the selection being dragged.
            return NO;
        }
        return YES;
    }
    
    return NO;
}

- (BOOL)_isMoveDrag
{
    return _private->initiatedDrag && 
    ([self _isEditable] && [self _hasSelection]) &&
    ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) == 0;
}

- (BOOL)_isNSColorDrag:(id <NSDraggingInfo>)draggingInfo
{
    return ([[[draggingInfo draggingPasteboard] types] containsObject:NSColorPboardType]);
}

- (NSDragOperation)draggingUpdatedWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    NSDragOperation operation = NSDragOperationNone;
    
    if (actionMask & WebDragDestinationActionDHTML) {
        operation = [[self _bridge] dragOperationForDraggingInfo:draggingInfo];
    }
    _private->webCoreHandlingDrag = (operation != NSDragOperationNone);
    
    if ((actionMask & WebDragDestinationActionEdit) &&
        !_private->webCoreHandlingDrag
        && [self _canProcessDragWithDraggingInfo:draggingInfo]) {
        if ([self _isNSColorDrag:draggingInfo]) {
            operation = NSDragOperationGeneric;
        }
        else {
            WebView *webView = [self _webView];
            [webView moveDragCaretToPoint:[webView convertPoint:[draggingInfo draggingLocation] fromView:nil]];
            operation = [self _isMoveDrag] ? NSDragOperationMove : NSDragOperationCopy;
        }
    } else {
        [[self _webView] removeDragCaret];
    }
    
    return operation;
}

- (void)draggingCancelledWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    [[self _bridge] dragExitedWithDraggingInfo:draggingInfo];
    [[self _webView] removeDragCaret];
}

- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    WebView *webView = [self _webView];
    WebBridge *bridge = [self _bridge];
    if (_private->webCoreHandlingDrag) {
        ASSERT(actionMask & WebDragDestinationActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionDHTML forDraggingInfo:draggingInfo];
        [bridge concludeDragForDraggingInfo:draggingInfo];
        return YES;
    } else if (actionMask & WebDragDestinationActionEdit) {
        if ([self _isNSColorDrag:draggingInfo]) {
            NSColor *color = [NSColor colorFromPasteboard:[draggingInfo draggingPasteboard]];
            if (!color)
                return NO;
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setProperty:@"color" :[self _colorAsString:color] :@""];
            if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:[bridge selectedDOMRange]]) {
                [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionEdit forDraggingInfo:draggingInfo];
                [bridge applyStyle:style withUndoAction:WebUndoActionSetColor];
                return YES;
            }
            return NO;
        }
        else {
            BOOL didInsert = NO;
            if ([self _canProcessDragWithDraggingInfo:draggingInfo]) {
                NSPasteboard *pasteboard = [draggingInfo draggingPasteboard];
                BOOL chosePlainText;
                DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard allowPlainText:YES chosePlainText:&chosePlainText];
                if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:[bridge dragCaretDOMRange] givenAction:WebViewInsertActionDropped]) {
                    [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionEdit forDraggingInfo:draggingInfo];
                    if ([self _isMoveDrag]) {
                        BOOL smartMove = [[self _bridge] selectionGranularity] == WebBridgeSelectByWord && [self _canSmartReplaceWithPasteboard:pasteboard];
                        [bridge moveSelectionToDragCaret:fragment smartMove:smartMove];
                    } else {
                        [bridge setSelectionToDragCaret];
                        [bridge replaceSelectionWithFragment:fragment selectReplacement:YES smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard] matchStyle:chosePlainText];
                    }
                    didInsert = YES;
                }
            }
            [webView removeDragCaret];
            return didInsert;
        }
    }
    return NO;
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [elementInfoWC mutableCopy];
    
    // Convert URL strings to NSURLs
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementLinkURLKey]] forKey:WebElementLinkURLKey];
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementImageURLKey]] forKey:WebElementImageURLKey];
    
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    ASSERT(webFrameView);
    WebFrame *webFrame = [webFrameView webFrame];
    
    if (webFrame) {
        NSString *frameName = [elementInfoWC objectForKey:WebElementLinkTargetFrameKey];
        if ([frameName length] == 0) {
            [elementInfo setObject:webFrame forKey:WebElementLinkTargetFrameKey];
        } else {
            WebFrame *wf = [webFrame findFrameNamed:frameName];
            if (wf != nil)
                [elementInfo setObject:wf forKey:WebElementLinkTargetFrameKey];
            else
                [elementInfo removeObjectForKey:WebElementLinkTargetFrameKey];
        }
        
        [elementInfo setObject:webFrame forKey:WebElementFrameKey];
    }
    
    return [elementInfo autorelease];
}

- (void)mouseUp:(NSEvent *)event
{
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    [self _stopAutoscrollTimer];
    [[self _bridge] mouseUp:event];
    [self _updateMouseoverWithFakeEvent];

    [self release];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (NSView *)nextValidKeyView
{
    NSView *view = nil;
    BOOL lookInsideWebFrameViews = YES;
    if ([self isHiddenOrHasHiddenAncestor]) {
        lookInsideWebFrameViews = NO;
    } else if ([self _frame] == [[self _webView] mainFrame]) {
        // Check for case where first responder is last frame in a frameset, and we are
        // the top-level documentView.
        NSResponder *firstResponder = [[self window] firstResponder];
        if ((firstResponder != self) && [firstResponder isKindOfClass:[WebHTMLView class]] && ([(NSView *)firstResponder nextKeyView] == nil)) {
            lookInsideWebFrameViews = NO;
        }
    }
    
    if (lookInsideWebFrameViews) {
        view = [[self _bridge] nextKeyViewInsideWebFrameViews];
    }
    
    if (view == nil) {
        view = [super nextValidKeyView];
        // If there's no next view wired up, we must be in the last subframe.
        // There's no direct link to the next valid key view; get it from the bridge.
        // Note that view == self here when nextKeyView returns nil, due to AppKit oddness.
        // We'll check for both nil and self in case the AppKit oddness goes away.
        // WebFrameView has this same kind of logic for the previousValidKeyView case.
        if (view == nil || view == self) {
            ASSERT([self _frame] != [[self _webView] mainFrame]);
            ASSERT(lookInsideWebFrameViews);
            view = [[self _bridge] nextValidKeyViewOutsideWebFrameViews];
        }
    }
        
    return view;
}

- (NSView *)previousValidKeyView
{
    NSView *view = nil;
    if (![self isHiddenOrHasHiddenAncestor]) {
        view = [[self _bridge] previousKeyViewInsideWebFrameViews];
    }
    if (view == nil) {
        view = [super previousValidKeyView];
    }
    return view;
}

- (BOOL)becomeFirstResponder
{
    NSView *view = nil;
    if (![[self _webView] _isPerformingProgrammaticFocus]) {
        switch ([[self window] keyViewSelectionDirection]) {
        case NSDirectSelection:
            break;
        case NSSelectingNext:
            view = [[self _bridge] nextKeyViewInsideWebFrameViews];
            break;
        case NSSelectingPrevious:
            view = [[self _bridge] previousKeyViewInsideWebFrameViews];
            break;
        }
    }
    if (view) {
        [[self window] makeFirstResponder:view];
    }
    [[self _webView] _selectedFrameDidChange];
    [self updateFocusState];
    [self _updateFontPanel];
    _private->startNewKillRingSequence = YES;
    return YES;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [_private->compController endRevertingChange:NO moveLeft:NO];
        _private->resigningFirstResponder = YES;
        if (![self maintainsInactiveSelection]) { 
            if ([[self _webView] _isPerformingProgrammaticFocus]) {
                [self deselectText];
            }
            else {
                [self deselectAll];
            }
        }
        [self updateFocusState];
        _private->resigningFirstResponder = NO;
    }
    return resign;
}

//------------------------------------------------------------------------------------
// WebDocumentView protocol
//------------------------------------------------------------------------------------
- (void)setDataSource:(WebDataSource *)dataSource 
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// This is an override of an NSControl method that wants to repaint the entire view when the window resigns/becomes
// key.  WebHTMLView is an NSControl only because it hosts NSCells that are painted by WebCore's Aqua theme
// renderer (and those cells must be hosted by an enclosing NSControl in order to paint properly).
- (void)updateCell:(NSCell*)cell
{
}

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:adjustViewSize];
        }
    }

    if (printing != _private->printing) {
        [_private->pageRects release];
        _private->pageRects = nil;
        _private->printing = printing;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
        if (!printing) {
            // Can't do this when starting printing or nested printing won't work, see 3491427.
            [self setNeedsDisplay:NO];
        }
    }
}

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode) {
        [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
    
    [[self _bridge] adjustPageHeightNew:newBottom top:oldTop bottom:oldBottom limit:bottomLimit];
    
    if (!wasInPrintingMode) {
        [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
}

- (float)_availablePaperWidthForPrintOperation:(NSPrintOperation *)printOperation
{
    NSPrintInfo *printInfo = [printOperation printInfo];
    return [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    float viewWidth = NSWidth([self bounds]);
    if (viewWidth < 1) {
        ERROR("%@ has no width when printing", self);
        return 1.0;
    }

    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1/PrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = [self _availablePaperWidthForPrintOperation:printOperation]/viewWidth;
    return userScaleFactor * MAX(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (float)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

// This is used for Carbon printing. At some point we might want to make this public API.
- (void)setPageWidthForPrinting:(float)pageWidth
{
    [self _setPrinting:NO minimumPageWidth:0. maximumPageWidth:0. adjustViewSize:NO];
    [self _setPrinting:YES minimumPageWidth:pageWidth maximumPageWidth:pageWidth adjustViewSize:YES];
}

- (void)_endPrintMode
{
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:YES];
    [[self window] setAutodisplay:YES];
}

- (void)_delayedEndPrintMode:(NSPrintOperation *)initiatingOperation
{
    ASSERT_ARG(initiatingOperation, initiatingOperation != nil);
    NSPrintOperation *currentOperation = [NSPrintOperation currentOperation];
    
    if (initiatingOperation == currentOperation) {
        // The print operation is still underway. We don't expect this to ever happen, hence the assert, but we're
        // being extra paranoid here since the printing code is so fragile. Delay the cleanup
        // further.
        ASSERT_NOT_REACHED();
        [self performSelector:@selector(_delayedEndPrintMode:) withObject:initiatingOperation afterDelay:0];
    } else if ([currentOperation view] == self) {
        // A new print job has started, but it is printing the same WebHTMLView again. We don't expect
        // this to ever happen, hence the assert, but we're being extra paranoid here since the printing code is so
        // fragile. Do nothing, because we don't want to break the print job currently in progress, and
        // the print job currently in progress is responsible for its own cleanup.
        ASSERT_NOT_REACHED();
    } else {
        // The print job that kicked off this delayed call has finished, and this view is not being
        // printed again. We expect that no other print job has started. Since this delayed call wasn't
        // cancelled, beginDocument and endDocument must not have been called, and we need to clean up
        // the print mode here.
        ASSERT(currentOperation == nil);
        [self _endPrintMode];
    }
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range {
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];
    
    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0;
    float maxLayoutWidth = 0.0;
    if (![[self _bridge] isFrameSet]) {
        float paperWidth = [self _availablePaperWidthForPrintOperation:[NSPrintOperation currentOperation]];
        minLayoutWidth = paperWidth*PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth*PrintingMaximumShrinkFactor;
    }
    [self _setPrinting:YES minimumPageWidth:minLayoutWidth maximumPageWidth:maxLayoutWidth adjustViewSize:YES]; // will relayout
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    // Certain types of errors, including invalid page ranges, can cause beginDocument and
    // endDocument to be skipped after we've put ourselves in print mode (see 4145905). In those cases
    // we need to get out of print mode without relying on any more callbacks from the printing mechanism.
    // If we get as far as beginDocument without trouble, then this delayed request will be cancelled.
    // If not cancelled, this delayed call will be invoked in the next pass through the main event loop,
    // which is after beginDocument and endDocument would be called.
    [self performSelector:@selector(_delayedEndPrintMode:) withObject:printOperation afterDelay:0];
    [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    [_private->pageRects release];
    NSArray *newPageRects = [[self _bridge] computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                          printHeight:floorf([self _calculatePrintHeight]/totalScaleFactor)];
    // AppKit gets all messed up if you give it a zero-length page count (see 3576334), so if we
    // hit that case we'll pass along a degenerate 1 pixel square to print. This will print
    // a blank page (with correct-looking header and footer if that option is on), which matches
    // the behavior of IE and Camino at least.
    if ([newPageRects count] == 0) {
        newPageRects = [NSArray arrayWithObject:[NSValue valueWithRect: NSMakeRect(0, 0, 1, 1)]];
    }
    _private->pageRects = [newPageRects retain];
    
    range->length = [_private->pageRects count];
    
    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(int)page {
    return [[_private->pageRects objectAtIndex: (page-1)] rectValue];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)beginDocument
{
    NS_DURING
        // From now on we'll get a chance to call _endPrintMode in either beginDocument or
        // endDocument, so we can cancel the "just in case" pending call.
        [NSObject cancelPreviousPerformRequestsWithTarget:self
                                                 selector:@selector(_delayedEndPrintMode:)
                                                   object:[NSPrintOperation currentOperation]];
        [super beginDocument];
    NS_HANDLER
        // Exception during [super beginDocument] means that endDocument will not get called,
        // so we need to clean up our "print mode" here.
        [self _endPrintMode];
    NS_ENDHANDLER
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _endPrintMode];
}

- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event
{   
    // Use the isEditable state to determine whether or not to process tab key events.
    // The idea here is that isEditable will be NO when this WebView is being used
    // in a browser, and we desire the behavior where tab moves to the next element
    // in tab order. If isEditable is YES, it is likely that the WebView is being
    // embedded as the whole view, as in Mail, and tabs should input tabs as expected
    // in a text editor.
    if (![[self _webView] isEditable] && [event _web_isTabKeyEvent]) 
        return NO;
    
    // Now process the key normally
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    [self retain];

    BOOL callSuper = NO;

    _private->keyDownEvent = event;

    WebBridge *bridge = [self _bridge];
    if ([bridge interceptKeyEvent:event toView:self]) {
        // WebCore processed a key event, bail on any outstanding complete: UI
        [_private->compController endRevertingChange:YES moveLeft:NO];
    } else if (_private->compController && [_private->compController filterKeyDown:event]) {
        // Consumed by complete: popup window
    } else {
        // We're going to process a key event, bail on any outstanding complete: UI
        [_private->compController endRevertingChange:YES moveLeft:NO];
        if ([self _canEdit] && [self _interceptEditingKeyEvent:event]) {
            // Consumed by key bindings manager.
        } else {
            callSuper = YES;
        }
    }
    if (callSuper) {
        [super keyDown:event];
    } else {
        [NSCursor setHiddenUntilMouseMoves:YES];
    }

    _private->keyDownEvent = nil;
    
    [self release];
}

- (void)keyUp:(NSEvent *)event
{
    [self retain];
    
    if (![[self _bridge] interceptKeyEvent:event toView:self]) {
        [super keyUp:event];
    }
    
    [self release];
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _bridge] accessibilityTree];
        if (accTree)
            return [NSArray arrayWithObject: accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

- (id)accessibilityFocusedUIElement
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree)
        return [accTree accessibilityFocusedUIElement];
    else
        return self;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree) {
        NSPoint windowCoord = [[self window] convertScreenToBase: point];
        return [accTree accessibilityHitTest: [self convertPoint:windowCoord fromView:nil]];
    }
    else
        return self;
}

- (id)_accessibilityParentForSubview:(NSView *)subview
{
    id accTree = [[self _bridge] accessibilityTree];
    if (!accTree)
        return self;
        
    id parent = [accTree _accessibilityParentForSubview:subview];
    if (parent == nil)
        return self;

    return parent;
}

- (void)centerSelectionInVisibleArea:(id)sender
{
    [[self _bridge] centerSelectionInVisibleArea];
}

- (void)_alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity
{
    WebBridge *bridge = [self _bridge];
    DOMRange *proposedRange = [bridge rangeByAlteringCurrentSelection:alteration direction:direction granularity:granularity];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange] toDOMRange:proposedRange affinity:[bridge selectionAffinity] stillSelecting:NO]) {
        [bridge alterCurrentSelection:alteration direction:direction granularity:granularity];
    }
}

- (void)_alterCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)verticalDistance
{
    WebBridge *bridge = [self _bridge];
    DOMRange *proposedRange = [bridge rangeByAlteringCurrentSelection:alteration verticalDistance:verticalDistance];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange] toDOMRange:proposedRange affinity:[bridge selectionAffinity] stillSelecting:NO]) {
        [bridge alterCurrentSelection:alteration verticalDistance:verticalDistance];
    }
}

- (void)moveBackward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectByCharacter];
}

- (void)moveBackwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectByCharacter];
}

- (void)moveDown:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectByLine];
}

- (void)moveDownAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectByLine];
}

- (void)moveForward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectByCharacter];
}

- (void)moveForwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectByCharacter];
}

- (void)moveLeft:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectLeft granularity:WebBridgeSelectByCharacter];
}

- (void)moveLeftAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectLeft granularity:WebBridgeSelectByCharacter];
}

- (void)moveRight:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectRight granularity:WebBridgeSelectByCharacter];
}

- (void)moveRightAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectRight granularity:WebBridgeSelectByCharacter];
}

- (void)moveToBeginningOfDocument:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectToDocumentBoundary];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectToDocumentBoundary];
}

- (void)moveToBeginningOfLine:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectToLineBoundary];
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectToLineBoundary];
}

- (void)moveToBeginningOfParagraph:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectToParagraphBoundary];
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectToParagraphBoundary];
}

- (void)moveToEndOfDocument:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectToDocumentBoundary];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectToDocumentBoundary];
}

- (void)moveToEndOfLine:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectToLineBoundary];
}

- (void)moveToEndOfLineAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectToLineBoundary];
}

- (void)moveToEndOfParagraph:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectToParagraphBoundary];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectToParagraphBoundary];
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectByParagraph];
}

- (void)moveParagraphForwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectByParagraph];
}

- (void)moveUp:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectByLine];
}

- (void)moveUpAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectByLine];
}

- (void)moveWordBackward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectBackward granularity:WebBridgeSelectByWord];
}

- (void)moveWordBackwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectByWord];
}

- (void)moveWordForward:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectForward granularity:WebBridgeSelectByWord];
}

- (void)moveWordForwardAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectForward granularity:WebBridgeSelectByWord];
}

- (void)moveWordLeft:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectLeft granularity:WebBridgeSelectByWord];
}

- (void)moveWordLeftAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectLeft granularity:WebBridgeSelectByWord];
}

- (void)moveWordRight:(id)sender
{
    [self _alterCurrentSelection:WebSelectByMoving direction:WebBridgeSelectRight granularity:WebBridgeSelectByWord];
}

- (void)moveWordRightAndModifySelection:(id)sender
{
    [self _alterCurrentSelection:WebSelectByExtending direction:WebBridgeSelectRight granularity:WebBridgeSelectByWord];
}

- (void)pageUp:(id)sender
{
    WebFrameView *frameView = [self _web_parentWebFrameView];
    if (frameView == nil)
        return;
    [self _alterCurrentSelection:WebSelectByMoving verticalDistance:-[frameView _verticalPageScrollDistance]];
}

- (void)pageDown:(id)sender
{
    WebFrameView *frameView = [self _web_parentWebFrameView];
    if (frameView == nil)
        return;
    [self _alterCurrentSelection:WebSelectByMoving verticalDistance:[frameView _verticalPageScrollDistance]];
}

- (void)pageUpAndModifySelection:(id)sender
{
    WebFrameView *frameView = [self _web_parentWebFrameView];
    if (frameView == nil)
        return;
    [self _alterCurrentSelection:WebSelectByExtending verticalDistance:-[frameView _verticalPageScrollDistance]];
}

- (void)pageDownAndModifySelection:(id)sender
{
    WebFrameView *frameView = [self _web_parentWebFrameView];
    if (frameView == nil)
        return;
    [self _alterCurrentSelection:WebSelectByExtending verticalDistance:[frameView _verticalPageScrollDistance]];
}

- (void)_expandSelectionToGranularity:(WebBridgeSelectionGranularity)granularity
{
    WebBridge *bridge = [self _bridge];
    DOMRange *range = [bridge rangeByExpandingSelectionWithGranularity:granularity];
    if (range && ![range collapsed]) {
        WebView *webView = [self _webView];
        if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange] toDOMRange:range affinity:[bridge selectionAffinity] stillSelecting:NO]) {
            [bridge setSelectedDOMRange:range affinity:[bridge selectionAffinity] closeTyping:YES];
        }
    }
}

- (void)selectParagraph:(id)sender
{
    [self _expandSelectionToGranularity:WebBridgeSelectByParagraph];
}

- (void)selectLine:(id)sender
{
    [self _expandSelectionToGranularity:WebBridgeSelectByLine];
}

- (void)selectWord:(id)sender
{
    [self _expandSelectionToGranularity:WebBridgeSelectByWord];
}

- (void)copy:(id)sender
{
    if ([[self _bridge] tryDHTMLCopy]) {
        return;     // DHTML did the whole operation
    }
    if (![self _canCopy]) {
        NSBeep();
        return;
    }
    [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
}

- (void)delete:(id)sender
{
    if (![self _canDelete]) {
        NSBeep();
        return;
    }
    [self _deleteSelection];
}

- (void)cut:(id)sender
{
    WebBridge *bridge = [self _bridge];
    if ([bridge tryDHTMLCut]) {
        return;     // DHTML did the whole operation
    }
    if (![self _canCut]) {
        NSBeep();
        return;
    }
    DOMRange *range = [self _selectedRange];
    if ([self _shouldDeleteRange:range]) {
        [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
        [bridge deleteSelectionWithSmartDelete:[self _canSmartCopyOrDelete]];
   }
}

- (void)paste:(id)sender
{
    if ([[self _bridge] tryDHTMLPaste]) {
        return;     // DHTML did the whole operation
    }
    if (![self _canPaste]) {
        return;
    }
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:YES];
}

- (NSData *)_selectionStartFontAttributesAsRTF
{
    NSAttributedString *string = [[NSAttributedString alloc] initWithString:@"x"
        attributes:[[self _bridge] fontAttributesForSelectionStart]];
    NSData *data = [string RTFFromRange:NSMakeRange(0, [string length]) documentAttributes:nil];
    [string release];
    return data;
}

- (NSDictionary *)_fontAttributesFromFontPasteboard
{
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    if (fontPasteboard == nil)
        return nil;
    NSData *data = [fontPasteboard dataForType:NSFontPboardType];
    if (data == nil || [data length] == 0)
        return nil;
    // NSTextView does something more efficient by parsing the attributes only, but that's not available in API.
    NSAttributedString *string = [[[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL] autorelease];
    if (string == nil || [string length] == 0)
        return nil;
    return [string fontAttributesInRange:NSMakeRange(0, 1)];
}

- (DOMCSSStyleDeclaration *)_emptyStyle
{
    return [[[self _bridge] DOMDocument] createCSSStyleDeclaration];
}

- (NSString *)_colorAsString:(NSColor *)color
{
    NSColor *rgbColor = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    // FIXME: If color is non-nil and rgbColor is nil, that means we got some kind
    // of fancy color that can't be converted to RGB. Changing that to "transparent"
    // might not be great, but it's probably OK.
    if (rgbColor == nil)
        return @"transparent";
    float r = [rgbColor redComponent];
    float g = [rgbColor greenComponent];
    float b = [rgbColor blueComponent];
    float a = [rgbColor alphaComponent];
    if (a == 0)
        return @"transparent";
    if (r == 0 && g == 0 && b == 0 && a == 1)
        return @"black";
    if (r == 1 && g == 1 && b == 1 && a == 1)
        return @"white";
    // FIXME: Lots more named colors. Maybe we could use the table in WebCore?
    if (a == 1)
        return [NSString stringWithFormat:@"rgb(%.0f,%.0f,%.0f)", r * 255, g * 255, b * 255];
    return [NSString stringWithFormat:@"rgba(%.0f,%.0f,%.0f,%f)", r * 255, g * 255, b * 255, a];
}

- (NSString *)_shadowAsString:(NSShadow *)shadow
{
    if (shadow == nil)
        return @"none";
    NSSize offset = [shadow shadowOffset];
    float blurRadius = [shadow shadowBlurRadius];
    if (offset.width == 0 && offset.height == 0 && blurRadius == 0)
        return @"none";
    NSColor *color = [shadow shadowColor];
    if (color == nil)
        return @"none";
    // FIXME: Handle non-integral values here?
    if (blurRadius == 0)
        return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height];
    return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height, blurRadius];
}

- (DOMCSSStyleDeclaration *)_styleFromFontAttributes:(NSDictionary *)dictionary
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSColor *color = [dictionary objectForKey:NSBackgroundColorAttributeName];
    [style setBackgroundColor:[self _colorAsString:color]];

    NSFont *font = [dictionary objectForKey:NSFontAttributeName];
    if (font == nil) {
        [style setFontFamily:@"Helvetica"];
        [style setFontSize:@"12px"];
        [style setFontWeight:@"normal"];
        [style setFontStyle:@"normal"];
    } else {
        NSFontManager *fm = [NSFontManager sharedFontManager];
        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", [font familyName]]];
        [style setFontSize:[NSString stringWithFormat:@"%0.fpx", [font pointSize]]];
        if ([fm weightOfFont:font] >= MIN_BOLD_WEIGHT) {
            [style setFontWeight:@"bold"];
        } else {
            [style setFontWeight:@"normal"];
        }
        if (([fm traitsOfFont:font] & NSItalicFontMask) != 0) {
            [style setFontStyle:@"italic"];
        } else {
            [style setFontStyle:@"normal"];
        }
    }

    color = [dictionary objectForKey:NSForegroundColorAttributeName];
    [style setColor:color ? [self _colorAsString:color] : (NSString *)@"black"];

    NSShadow *shadow = [dictionary objectForKey:NSShadowAttributeName];
    [style setTextShadow:[self _shadowAsString:shadow]];

    int strikethroughInt = [[dictionary objectForKey:NSStrikethroughStyleAttributeName] intValue];

    int superscriptInt = [[dictionary objectForKey:NSSuperscriptAttributeName] intValue];
    if (superscriptInt > 0)
        [style setVerticalAlign:@"super"];
    else if (superscriptInt < 0)
        [style setVerticalAlign:@"sub"];
    else
        [style setVerticalAlign:@"baseline"];
    int underlineInt = [[dictionary objectForKey:NSUnderlineStyleAttributeName] intValue];
    // FIXME: Underline wins here if we have both (see bug 3790443).
    if (strikethroughInt == NSUnderlineStyleNone && underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" :@"none" :@""];
    else if (underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" :@"line-through" :@""];
    else
        [style setProperty:@"-khtml-text-decorations-in-effect" :@"underline" :@""];

    return style;
}

- (void)_applyStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (style == nil || [style length] == 0 || ![self _canEdit])
        return;
    WebView *webView = [self _webView];
    WebBridge *bridge = [self _bridge];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:[self _selectedRange]]) {
        [bridge applyStyle:style withUndoAction:undoAction];
    }
}

- (void)_applyParagraphStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction
{
    if (style == nil || [style length] == 0 || ![self _canEdit])
        return;
    WebView *webView = [self _webView];
    WebBridge *bridge = [self _bridge];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:[self _selectedRange]]) {
        [bridge applyParagraphStyle:style withUndoAction:undoAction];
    }
}

- (void)_toggleBold
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setFontWeight:@"bold"];
    if ([[self _bridge] selectionStartHasStyle:style])
        [style setFontWeight:@"normal"];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionSetFont];
}

- (void)_toggleItalic
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setFontStyle:@"italic"];
    if ([[self _bridge] selectionStartHasStyle:style])
        [style setFontStyle:@"normal"];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionSetFont];
}

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    ASSERT([self _webView]);
    if (![[[self _webView] preferences] respectStandardStyleKeyEquivalents]) {
        return NO;
    }
    
    if (![self _canEdit])
        return NO;
    
    NSString *string = [event charactersIgnoringModifiers];
    if ([string isEqualToString:@"b"]) {
        [self _toggleBold];
        return YES;
    }
    if ([string isEqualToString:@"i"]) {
        [self _toggleItalic];
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if ([self _handleStyleKeyEquivalent:event]) {
        return YES;
    }
    
    BOOL ret;
    
    [self retain];
    
    // Pass command-key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting command-modified keypresses.
    // But don't do it if we have already handled the event.
    if (event != _private->keyDownEvent
            && [self _web_firstResponderIsSelfOrDescendantView]
            && [[self _bridge] interceptKeyEvent:event toView:self]) {
        
        ret = YES;
    }
    else
        ret = [super performKeyEquivalent:event];
    
    [self release];
    
    return ret;
}

- (void)copyFont:(id)sender
{
    // Put RTF with font attributes on the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    [fontPasteboard declareTypes:[NSArray arrayWithObject:NSFontPboardType] owner:nil];
    [fontPasteboard setData:[self _selectionStartFontAttributesAsRTF] forType:NSFontPboardType];
}

- (void)pasteFont:(id)sender
{
    // Read RTF with font attributes from the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    [self _applyStyleToSelection:[self _styleFromFontAttributes:[self _fontAttributesFromFontPasteboard]] withUndoAction:WebUndoActionPasteFont];
}

- (void)pasteAsPlainText:(id)sender
{
    if (![self _canEdit])
        return;
        
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *text = [pasteboard stringForType:NSStringPboardType];
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithText:text selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard]];
    }
}

- (void)pasteAsRichText:(id)sender
{
    // Since rich text always beats plain text when both are on the pasteboard, it's not
    // clear how this is different from plain old paste.
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:NO];
}

- (NSFont *)_originalFontA
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Helvetica" traits:0 weight:STANDARD_WEIGHT size:10];
}

- (NSFont *)_originalFontB
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Times" traits:(NSBoldFontMask | NSItalicFontMask) weight:STANDARD_BOLD_WEIGHT size:12];
}

- (void)_addToStyle:(DOMCSSStyleDeclaration *)style fontA:(NSFont *)a fontB:(NSFont *)b
{
    // Since there's no way to directly ask NSFontManager what style change it's going to do
    // we instead pass two "specimen" fonts to it and let it change them. We then deduce what
    // style change it was doing by looking at what happened to each of the two fonts.
    // So if it was making the text bold, both fonts will be bold after the fact.

    if (a == nil || b == nil)
        return;

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];

    NSString *aFamilyName = [a familyName];
    NSString *bFamilyName = [b familyName];

    int aPointSize = [a pointSize];
    int bPointSize = [b pointSize];

    int aWeight = [fm weightOfFont:a];
    int bWeight = [fm weightOfFont:b];

    BOOL aIsBold = aWeight >= MIN_BOLD_WEIGHT;

    BOOL aIsItalic = ([fm traitsOfFont:a] & NSItalicFontMask) != 0;
    BOOL bIsItalic = ([fm traitsOfFont:b] & NSItalicFontMask) != 0;

    if ([aFamilyName isEqualToString:bFamilyName]) {
        NSString *familyNameForCSS = aFamilyName;

        // The family name may not be specific enough to get us the font specified.
        // In some cases, the only way to get exactly what we are looking for is to use
        // the Postscript name.
        
        // Find the font the same way the rendering code would later if it encountered this CSS.
        WebTextRendererFactory *factory = [WebTextRendererFactory sharedFactory];
        NSFontTraitMask traits = 0;
        if (aIsBold)
            traits |= NSBoldFontMask;
        if (aIsItalic)
            traits |= NSItalicFontMask;
        NSFont *foundFont = [factory cachedFontFromFamily:aFamilyName traits:traits size:aPointSize];

        // If we don't find a font with the same Postscript name, then we'll have to use the
        // Postscript name to make the CSS specific enough.
        if (![[foundFont fontName] isEqualToString:[a fontName]]) {
            familyNameForCSS = [a fontName];
        }

        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", familyNameForCSS]];
    }

    int soa = [oa pointSize];
    if (aPointSize == bPointSize) {
        [style setFontSize:[NSString stringWithFormat:@"%dpx", aPointSize]];
    } else if (aPointSize < soa) {
        [style _setFontSizeDelta:@"-1px"];
    } else if (aPointSize > soa) {
        [style _setFontSizeDelta:@"1px"];
    }

    if (aWeight == bWeight) {
        if (aIsBold) {
            [style setFontWeight:@"bold"];
        } else {
            [style setFontWeight:@"normal"];
        }
    }

    if (aIsItalic == bIsItalic) {
        if (aIsItalic) {
            [style setFontStyle:@"italic"];
        } else {
            [style setFontStyle:@"normal"];
        }
    }
}

- (DOMCSSStyleDeclaration *)_styleFromFontManagerOperation
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];
    NSFont *ob = [self _originalFontB];    
    [self _addToStyle:style fontA:[fm convertFont:oa] fontB:[fm convertFont:ob]];

    return style;
}

- (void)changeFont:(id)sender
{
    [self _applyStyleToSelection:[self _styleFromFontManagerOperation] withUndoAction:WebUndoActionSetFont];
}

- (DOMCSSStyleDeclaration *)_styleForAttributeChange:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSShadow *shadow = [[NSShadow alloc] init];
    [shadow setShadowOffset:NSMakeSize(1, 1)];

    NSDictionary *oa = [NSDictionary dictionaryWithObjectsAndKeys:
        [self _originalFontA], NSFontAttributeName,
        nil];
    NSDictionary *ob = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSColor blackColor], NSBackgroundColorAttributeName,
        [self _originalFontB], NSFontAttributeName,
        [NSColor whiteColor], NSForegroundColorAttributeName,
        shadow, NSShadowAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSStrikethroughStyleAttributeName,
        [NSNumber numberWithInt:1], NSSuperscriptAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
        nil];

    [shadow release];

#if 0

NSObliquenessAttributeName        /* float; skew to be applied to glyphs, default 0: no skew */
    // font-style, but that is just an on-off switch

NSExpansionAttributeName          /* float; log of expansion factor to be applied to glyphs, default 0: no expansion */
    // font-stretch?

NSKernAttributeName               /* float, amount to modify default kerning, if 0, kerning off */
    // letter-spacing? probably not good enough

NSUnderlineColorAttributeName     /* NSColor, default nil: same as foreground color */
NSStrikethroughColorAttributeName /* NSColor, default nil: same as foreground color */
    // text-decoration-color?

NSLigatureAttributeName           /* int, default 1: default ligatures, 0: no ligatures, 2: all ligatures */
NSBaselineOffsetAttributeName     /* float, in points; offset from baseline, default 0 */
NSStrokeWidthAttributeName        /* float, in percent of font point size, default 0: no stroke; positive for stroke alone, negative for stroke and fill (a typical value for outlined text would be 3.0) */
NSStrokeColorAttributeName        /* NSColor, default nil: same as foreground color */
    // need extensions?

#endif
    
    NSDictionary *a = [sender convertAttributes:oa];
    NSDictionary *b = [sender convertAttributes:ob];

    NSColor *ca = [a objectForKey:NSBackgroundColorAttributeName];
    NSColor *cb = [b objectForKey:NSBackgroundColorAttributeName];
    if (ca == cb) {
        [style setBackgroundColor:[self _colorAsString:ca]];
    }

    [self _addToStyle:style fontA:[a objectForKey:NSFontAttributeName] fontB:[b objectForKey:NSFontAttributeName]];

    ca = [a objectForKey:NSForegroundColorAttributeName];
    cb = [b objectForKey:NSForegroundColorAttributeName];
    if (ca == cb) {
        [style setColor:[self _colorAsString:ca]];
    }

    NSShadow *sha = [a objectForKey:NSShadowAttributeName];
    if (sha) {
        [style setTextShadow:[self _shadowAsString:sha]];
    } else if ([b objectForKey:NSShadowAttributeName] == nil) {
        [style setTextShadow:@"none"];
    }

    int sa = [[a objectForKey:NSStrikethroughStyleAttributeName] intValue];
    int sb = [[b objectForKey:NSStrikethroughStyleAttributeName] intValue];
    if (sa == sb) {
        if (sa == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" :@"none" :@""]; 
            // we really mean "no line-through" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" :@"line-through" :@""];
            // we really mean "add line-through" rather than "line-through"
    }

    sa = [[a objectForKey:NSSuperscriptAttributeName] intValue];
    sb = [[b objectForKey:NSSuperscriptAttributeName] intValue];
    if (sa == sb) {
        if (sa > 0)
            [style setVerticalAlign:@"super"];
        else if (sa < 0)
            [style setVerticalAlign:@"sub"];
        else
            [style setVerticalAlign:@"baseline"];
    }

    int ua = [[a objectForKey:NSUnderlineStyleAttributeName] intValue];
    int ub = [[b objectForKey:NSUnderlineStyleAttributeName] intValue];
    if (ua == ub) {
        if (ua == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" :@"none" :@""];
            // we really mean "no underline" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" :@"underline" :@""];
            // we really mean "add underline" rather than "underline"
    }

    return style;
}

- (void)changeAttributes:(id)sender
{
    [self _applyStyleToSelection:[self _styleForAttributeChange:sender] withUndoAction:WebUndoActionChangeAttributes];
}

- (DOMCSSStyleDeclaration *)_styleFromColorPanelWithSelector:(SEL)selector
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    ASSERT([style respondsToSelector:selector]);
    [style performSelector:selector withObject:[self _colorAsString:[[NSColorPanel sharedColorPanel] color]]];
    
    return style;
}

- (WebUndoAction)_undoActionFromColorPanelWithSelector:(SEL)selector
{
    if (selector == @selector(setBackgroundColor:)) {
        return WebUndoActionSetBackgroundColor;
    }
    
    return WebUndoActionSetColor;
}

- (void)_changeCSSColorUsingSelector:(SEL)selector inRange:(DOMRange *)range
{
    DOMCSSStyleDeclaration *style = [self _styleFromColorPanelWithSelector:selector];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:range]) {
        [[self _bridge] applyStyle:style withUndoAction:[self _undoActionFromColorPanelWithSelector:selector]];
    }
}

- (void)changeDocumentBackgroundColor:(id)sender
{
    // Mimicking NSTextView, this method sets the background color for the
    // entire document. There is no NSTextView API for setting the background
    // color on the selected range only. Note that this method is currently
    // never called from the UI (see comment in changeColor:).
    // FIXME: this actually has no effect when called, probably due to 3654850. _documentRange seems
    // to do the right thing because it works in startSpeaking:, and I know setBackgroundColor: does the
    // right thing because I tested it with [self _selectedRange].
    // FIXME: This won't actually apply the style to the entire range here, because it ends up calling
    // [bridge applyStyle:], which operates on the current selection. To make this work right, we'll
    // need to save off the selection, temporarily set it to the entire range, make the change, then
    // restore the old selection.
    [self _changeCSSColorUsingSelector:@selector(setBackgroundColor:) inRange:[self _documentRange]];
}

- (void)changeColor:(id)sender
{
    // FIXME: in NSTextView, this method calls changeDocumentBackgroundColor: when a
    // private call has earlier been made by [NSFontFontEffectsBox changeColor:], see 3674493. 
    // AppKit will have to be revised to allow this to work with anything that isn't an 
    // NSTextView. However, this might not be required for Tiger, since the background-color 
    // changing box in the font panel doesn't work in Mail (3674481), though it does in TextEdit.
    [self _applyStyleToSelection:[self _styleFromColorPanelWithSelector:@selector(setColor:)] 
                  withUndoAction:WebUndoActionSetColor];
}

- (void)_alignSelectionUsingCSSValue:(NSString *)CSSAlignmentValue withUndoAction:(WebUndoAction)undoAction
{
    if (![self _canEdit])
        return;
        
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setTextAlign:CSSAlignmentValue];
    [self _applyStyleToSelection:style withUndoAction:undoAction];
}

- (void)alignCenter:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"center" withUndoAction:WebUndoActionCenter];
}

- (void)alignJustified:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"justify" withUndoAction:WebUndoActionJustify];
}

- (void)alignLeft:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"left" withUndoAction:WebUndoActionAlignLeft];
}

- (void)alignRight:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"right" withUndoAction:WebUndoActionAlignRight];
}

- (void)insertTab:(id)sender
{
    [self insertText:@"\t"];
}

- (void)insertBacktab:(id)sender
{
    // Doing nothing matches normal NSTextView behavior. If we ever use WebView for a field-editor-type purpose
    // we might add code here.
}

- (void)insertNewline:(id)sender
{
    if (![self _canEdit])
        return;
        
    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        [bridge insertParagraphSeparator];
    }
}

- (void)insertLineBreak:(id)sender
{
    if (![self _canEdit])
        return;
        
    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        [bridge insertLineBreak];
    }
}

- (void)insertParagraphSeparator:(id)sender
{
    if (![self _canEdit])
        return;

    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        [bridge insertParagraphSeparator];
    }
}

- (void)_changeWordCaseWithSelector:(SEL)selector
{
    if (![self _canEdit])
        return;

    WebBridge *bridge = [self _bridge];
    [self selectWord:nil];
    NSString *word = [[bridge selectedString] performSelector:selector];
    // FIXME: Does this need a different action context other than "typed"?
    if ([self _shouldReplaceSelectionWithText:word givenAction:WebViewInsertActionTyped]) {
        [bridge replaceSelectionWithText:word selectReplacement:NO smartReplace:NO];
    }
}

- (void)uppercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(uppercaseString)];
}

- (void)lowercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(lowercaseString)];
}

- (void)capitalizeWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(capitalizedString)];
}

- (BOOL)_deleteWithDirection:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity killRing:(BOOL)killRing isTypingAction:(BOOL)isTypingAction
{
    // Delete the selection, if there is one.
    // If not, make a selection using the passed-in direction and granularity.

    if (![self _canEdit])
        return NO;

    DOMRange *range;
    WebDeletionAction deletionAction = deleteSelectionAction;

    if ([self _hasSelection]) {
        range = [self _selectedRange];
        if (isTypingAction)
            deletionAction = deleteKeyAction;
    } else {
        range = [[self _bridge] rangeByAlteringCurrentSelection:WebSelectByExtending direction:direction granularity:granularity];
        if (isTypingAction)
            switch (direction) {
                case WebBridgeSelectForward:
                case WebBridgeSelectRight:
                    deletionAction = forwardDeleteKeyAction;
                    break;
                case WebBridgeSelectBackward:
                case WebBridgeSelectLeft:
                    deletionAction = deleteKeyAction;
                    break;
            }
    }

    if (range == nil || [range collapsed] || ![self _shouldDeleteRange:range])
        return NO;
    [self _deleteRange:range killRing:killRing prepend:NO smartDeleteOK:YES deletionAction:deletionAction];
    return YES;
}

- (void)deleteForward:(id)sender
{
    if (![self _isEditable])
        return;
    [self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectByCharacter killRing:NO isTypingAction:YES];
}

- (void)deleteBackward:(id)sender
{
    if (![self _isEditable])
        return;
    [self _deleteWithDirection:WebBridgeSelectBackward granularity:WebBridgeSelectByCharacter killRing:NO isTypingAction:YES];
}

- (void)deleteBackwardByDecomposingPreviousCharacter:(id)sender
{
    ERROR("unimplemented, doing deleteBackward instead");
    [self deleteBackward:sender];
}

- (void)deleteWordForward:(id)sender
{
    [self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectByWord killRing:YES isTypingAction:NO];
}

- (void)deleteWordBackward:(id)sender
{
    [self _deleteWithDirection:WebBridgeSelectBackward granularity:WebBridgeSelectByWord killRing:YES isTypingAction:NO];
}

- (void)deleteToBeginningOfLine:(id)sender
{
    [self _deleteWithDirection:WebBridgeSelectBackward granularity:WebBridgeSelectToLineBoundary killRing:YES isTypingAction:NO];
}

- (void)deleteToEndOfLine:(id)sender
{
    // To match NSTextView, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like deleteToEndOfParagraph does below).
    if(![self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectToLineBoundary killRing:YES isTypingAction:NO])
        [self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectByCharacter killRing:YES isTypingAction:NO];
}

- (void)deleteToBeginningOfParagraph:(id)sender
{
    [self _deleteWithDirection:WebBridgeSelectBackward granularity:WebBridgeSelectToParagraphBoundary killRing:YES isTypingAction:NO];
}

- (void)deleteToEndOfParagraph:(id)sender
{
    // Despite the name of the method, this should delete the newline if the caret is at the end of a paragraph.
    // If deletion to the end of the paragraph fails, we delete one character forward, which will delete the newline.
    if (![self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectToParagraphBoundary killRing:YES isTypingAction:NO])
        [self _deleteWithDirection:WebBridgeSelectForward granularity:WebBridgeSelectByCharacter killRing:YES isTypingAction:NO];
}

- (void)complete:(id)sender
{
    if (![self _canEdit])
        return;

    if (!_private->compController) {
        _private->compController = [[WebTextCompleteController alloc] initWithHTMLView:self];
    }
    [_private->compController doCompletion];
}

- (void)checkSpelling:(id)sender
{
    // WebCore does everything but update the spelling panel
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }
    NSString *badWord = [[self _bridge] advanceToNextMisspelling];
    if (badWord) {
        [checker updateSpellingPanelWithMisspelledWord:badWord];
    }
}

- (void)showGuessPanel:(id)sender
{
    // WebCore does everything but update the spelling panel
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }
    NSString *badWord = [[self _bridge] advanceToNextMisspellingStartingJustBeforeSelection];
    if (badWord) {
        [checker updateSpellingPanelWithMisspelledWord:badWord];
    }
    [[checker spellingPanel] orderFront:sender];
}

- (void)_changeSpellingToWord:(NSString *)newWord
{
    if (![self _canEdit])
        return;

    // Don't correct to empty string.  (AppKit checked this, we might as well too.)
    if (![NSSpellChecker sharedSpellChecker]) {
        ERROR("No NSSpellChecker");
        return;
    }
    
    if ([newWord isEqualToString:@""]) {
        return;
    }

    if ([self _shouldReplaceSelectionWithText:newWord givenAction:WebViewInsertActionPasted]) {
        [[self _bridge] replaceSelectionWithText:newWord selectReplacement:YES smartReplace:NO];
    }
}

- (void)changeSpelling:(id)sender
{
    [self _changeSpellingToWord:[[sender selectedCell] stringValue]];
}

- (void)ignoreSpelling:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        ERROR("No NSSpellChecker");
        return;
    }
    
    NSString *stringToIgnore = [sender stringValue];
    unsigned int length = [stringToIgnore length];
    if (stringToIgnore && length > 0) {
        [checker ignoreWord:stringToIgnore inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]];
        // FIXME: Need to clear misspelling marker if the currently selected word is the one we are to ignore?
    }
}

- (void)performFindPanelAction:(id)sender
{
    // Implementing this will probably require copying all of NSFindPanel.h and .m.
    // We need *almost* the same thing as AppKit, but not quite.
    ERROR("unimplemented");
}

- (void)startSpeaking:(id)sender
{
    WebBridge *bridge = [self _bridge];
    DOMRange *range = [self _selectedRange];
    if (!range || [range collapsed]) {
        range = [self _documentRange];
    }
    [NSApp speakString:[bridge stringForRange:range]];
}

- (void)stopSpeaking:(id)sender
{
    [NSApp stopSpeaking:sender];
}

- (void)insertNewlineIgnoringFieldEditor:(id)sender
{
    [self insertNewline:sender];
}

- (void)insertTabIgnoringFieldEditor:(id)sender
{
    [self insertTab:sender];
}

- (void)subscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"sub"];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionSubscript];
}

- (void)superscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"super"];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionSuperscript];
}

- (void)unscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"baseline"];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionUnscript];
}

- (void)underline:(id)sender
{
    // Despite the name, this method is actually supposed to toggle underline.
    // FIXME: This currently clears overline, line-through, and blink as an unwanted side effect.
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setProperty:@"-khtml-text-decorations-in-effect" :@"underline" :@""];
    if ([[self _bridge] selectionStartHasStyle:style])
        [style setProperty:@"-khtml-text-decorations-in-effect" :@"none" :@""];
    [self _applyStyleToSelection:style withUndoAction:WebUndoActionUnderline];
}

- (void)yank:(id)sender
{
    if (![self _canEdit])
        return;
        
    [self insertText:_NSYankFromKillRing()];
    _NSSetKillRingToYankedState();
}

- (void)yankAndSelect:(id)sender
{
    if (![self _canEdit])
        return;

    [self _insertText:_NSYankPreviousFromKillRing() selectInsertedText:YES];
    _NSSetKillRingToYankedState();
}

- (void)setMark:(id)sender
{
    [[self _bridge] setMarkDOMRange:[self _selectedRange]];
}

static DOMRange *unionDOMRanges(DOMRange *a, DOMRange *b)
{
    ASSERT(a);
    ASSERT(b);
    DOMRange *s = [a compareBoundaryPoints:DOM_START_TO_START :b] <= 0 ? a : b;
    DOMRange *e = [a compareBoundaryPoints:DOM_END_TO_END :b] <= 0 ? b : a;
    DOMRange *r = [[[a startContainer] ownerDocument] createRange];
    [r setStart:[s startContainer] :[s startOffset]];
    [r setEnd:[e endContainer] :[e endOffset]];
    return r;
}

- (void)deleteToMark:(id)sender
{
    if (![self _canEdit])
        return;

    DOMRange *mark = [[self _bridge] markDOMRange];
    if (mark == nil) {
        [self delete:sender];
    } else {
        DOMRange *selection = [self _selectedRange];
        DOMRange *r;
        NS_DURING
            r = unionDOMRanges(mark, selection);
        NS_HANDLER
            r = selection;
        NS_ENDHANDLER
        [self _deleteRange:r killRing:YES prepend:YES smartDeleteOK:NO deletionAction:deleteSelectionAction];
    }
    [self setMark:sender];
}

- (void)selectToMark:(id)sender
{
    WebBridge *bridge = [self _bridge];
    DOMRange *mark = [bridge markDOMRange];
    if (mark == nil) {
        NSBeep();
        return;
    }
    DOMRange *selection = [self _selectedRange];
    NS_DURING
        [bridge setSelectedDOMRange:unionDOMRanges(mark, selection) affinity:NSSelectionAffinityDownstream closeTyping:YES];
    NS_HANDLER
        NSBeep();
    NS_ENDHANDLER
}

- (void)swapWithMark:(id)sender
{
    WebBridge *bridge = [self _bridge];
    DOMRange *mark = [bridge markDOMRange];
    if (mark == nil) {
        NSBeep();
        return;
    }
    DOMRange *selection = [self _selectedRange];
    NS_DURING
        [bridge setSelectedDOMRange:mark affinity:NSSelectionAffinityDownstream closeTyping:YES];
    NS_HANDLER
        NSBeep();
        return;
    NS_ENDHANDLER
    [bridge setMarkDOMRange:selection];
}

- (void)transpose:(id)sender
{
    if (![self _canEdit])
        return;

    WebBridge *bridge = [self _bridge];
    DOMRange *r = [bridge rangeOfCharactersAroundCaret];
    if (!r) {
        return;
    }
    NSString *characters = [bridge stringForRange:r];
    if ([characters length] != 2) {
        return;
    }
    NSString *transposed = [[characters substringFromIndex:1] stringByAppendingString:[characters substringToIndex:1]];
    WebView *webView = [self _webView];
    if (![[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange] toDOMRange:r affinity:NSSelectionAffinityDownstream stillSelecting:NO]) {
        return;
    }
    [bridge setSelectedDOMRange:r affinity:NSSelectionAffinityDownstream closeTyping:YES];
    if ([self _shouldReplaceSelectionWithText:transposed givenAction:WebViewInsertActionTyped]) {
        [bridge replaceSelectionWithText:transposed selectReplacement:NO smartReplace:NO];
    }
}

- (void)toggleBaseWritingDirection:(id)sender
{
    if (![self _canEdit])
        return;
    
    NSString *direction = @"RTL";
    switch ([[self _bridge] baseWritingDirectionForSelectionStart]) {
        case NSWritingDirectionLeftToRight:
            break;
        case NSWritingDirectionRightToLeft:
            direction = @"LTR";
            break;
        // The writingDirectionForSelectionStart method will never return "natural". It
        // will always return a concrete direction. So, keep the compiler happy, and assert not reached.
        case NSWritingDirectionNatural:
            ASSERT_NOT_REACHED();
            break;
    }

    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setDirection:direction];
    [self _applyParagraphStyleToSelection:style withUndoAction:WebUndoActionSetWritingDirection];
}

- (void)changeBaseWritingDirection:(id)sender
{
    if (![self _canEdit])
        return;
    
    NSWritingDirection writingDirection = [sender tag];

    NSString *direction = @"LTR";
    switch (writingDirection) {
        case NSWritingDirectionLeftToRight:
        case NSWritingDirectionNatural:
            break;
        case NSWritingDirectionRightToLeft:
            direction = @"RTL";
            break;
    }

    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setDirection:direction];
    [self _applyParagraphStyleToSelection:style withUndoAction:WebUndoActionSetWritingDirection];
}

#if 0

// CSS does not have a way to specify an outline font, which may make this difficult to implement.
// Maybe a special case of text-shadow?
- (void)outline:(id)sender;

// This is part of table support, which may be in NSTextView for Tiger.
// It's probably simple to do the equivalent thing for WebKit.
- (void)insertTable:(id)sender;

// === key binding methods that NSTextView has that don't have standard key bindings

// These could be important.
- (void)toggleTraditionalCharacterShape:(id)sender;

// I'm not sure what the equivalents of these in the web world are.
- (void)insertLineSeparator:(id)sender;
- (void)insertPageBreak:(id)sender;

// === methods not present in NSTextView

// These methods are not implemented in NSTextView yet, so perhaps there's no rush.
- (void)changeCaseOfLetter:(id)sender;
- (void)indent:(id)sender;
- (void)transposeWords:(id)sender;

#endif

// Super-hack alert.
// Workaround for bug 3789278.

// Returns a selector only if called while:
//   1) first responder is self
//   2) handling a key down event
//   3) not yet inside keyDown: method
//   4) key is an arrow key
// The selector is the one that gets sent by -[NSWindow _processKeyboardUIKey] for this key.
- (SEL)_arrowKeyDownEventSelectorIfPreprocessing
{
    NSWindow *w = [self window];
    if ([w firstResponder] != self)
        return NULL;
    NSEvent *e = [w currentEvent];
    if ([e type] != NSKeyDown)
        return NULL;
    if (e == _private->keyDownEvent)
        return NULL;
    NSString *s = [e charactersIgnoringModifiers];
    if ([s length] == 0)
        return NULL;
    switch ([s characterAtIndex:0]) {
        case NSDownArrowFunctionKey:
            return @selector(moveDown:);
        case NSLeftArrowFunctionKey:
            return @selector(moveLeft:);
        case NSRightArrowFunctionKey:
            return @selector(moveRight:);
        case NSUpArrowFunctionKey:
            return @selector(moveUp:);
        default:
            return NULL;
    }
}

// Returns NO instead of YES if called on the selector that the
// _arrowKeyDownEventSelectorIfPreprocessing method returns.
// This should only happen inside -[NSWindow _processKeyboardUIKey],
// and together with the change below should cause that method
// to return NO rather than handling the key.
// Also set a 1-shot flag for the nextResponder check below.
- (BOOL)respondsToSelector:(SEL)selector
{
    if (![super respondsToSelector:selector])
        return NO;
    SEL arrowKeySelector = [self _arrowKeyDownEventSelectorIfPreprocessing];
    if (selector != arrowKeySelector)
        return YES;
    _private->nextResponderDisabledOnce = YES;
    return NO;
}

// Returns nil instead of the next responder if called when the
// one-shot flag is set, and _arrowKeyDownEventSelectorIfPreprocessing
// returns something other than NULL. This should only happen inside
// -[NSWindow _processKeyboardUIKey] and together with the change above
// should cause that method to return NO rather than handling the key.
- (NSResponder *)nextResponder
{
    BOOL disabled = _private->nextResponderDisabledOnce;
    _private->nextResponderDisabledOnce = NO;
    if (disabled && [self _arrowKeyDownEventSelectorIfPreprocessing] != NULL) {
        return nil;
    }
    return [super nextResponder];
}

@end

@implementation WebHTMLView (WebTextSizing)

- (IBAction)_makeTextSmaller:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextLarger:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextStandardSize:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (BOOL)_tracksCommonSizeFactor
{
    return YES;
}

// never sent because we track the common size factor
- (BOOL)_canMakeTextSmaller          {   ASSERT_NOT_REACHED(); return NO;    }
- (BOOL)_canMakeTextLarger           {   ASSERT_NOT_REACHED(); return NO;    }
- (BOOL)_canMakeTextStandardSize     {   ASSERT_NOT_REACHED(); return NO;    }

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginEmbeddedView *view;
    while ((view = [enumerator nextObject]) != nil) {
        if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]]) {
            [view performSelector:selector withObject:object];
        }
    }
}

@end

@implementation WebHTMLView (WebInternal)

- (void)_selectionChanged
{
    [self _updateSelectionForInputManager];
    [self _updateFontPanel];
    _private->startNewKillRingSequence = YES;
}

- (void)_formControlIsResigningFirstResponder:(NSView *)formControl
{
    // set resigningFirstResponder so updateFocusState behaves the same way it does when
    // the WebHTMLView itself is resigningFirstResponder; don't use the primary selection feedback.
    // If the first responder is in the process of being set on the WebHTMLView itself, it will
    // get another chance at updateFocusState in its own becomeFirstResponder method.
    _private->resigningFirstResponder = YES;
    [self updateFocusState];
    _private->resigningFirstResponder = NO;
}

- (void)_updateFontPanel
{
    // FIXME: NSTextView bails out if becoming or resigning first responder, for which it has ivar flags. Not
    // sure if we need to do something similar.
    
    if (![self _canEdit])
        return;
    
    NSWindow *window = [self window];
    // FIXME: is this first-responder check correct? What happens if a subframe is editable and is first responder?
    if ([NSApp keyWindow] != window || [window firstResponder] != self) {
        return;
    }
    
    BOOL multiple = NO;
    NSFont *font = [[self _bridge] fontForSelection:&multiple];

    // FIXME: for now, return a bogus font that distinguishes the empty selection from the non-empty
    // selection. We should be able to remove this once the rest of this code works properly.
    if (font == nil) {
        if (![self _hasSelection]) {
            font = [NSFont toolTipsFontOfSize:17];
        } else {
            font = [NSFont menuFontOfSize:23];
        }
    }
    ASSERT(font != nil);

    NSFontManager *fm = [NSFontManager sharedFontManager];
    [fm setSelectedFont:font isMultiple:multiple];

    // FIXME: we don't keep track of selected attributes, or set them on the font panel. This
    // appears to have no effect on the UI. E.g., underlined text in Mail or TextEdit is
    // not reflected in the font panel. Maybe someday this will change.
}

- (unsigned int)_delegateDragSourceActionMask
{
    ASSERT(_private->mouseDownEvent != nil);
    WebView *webView = [self _webView];
    NSPoint point = [webView convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    _private->dragSourceActionMask = [[webView _UIDelegateForwarder] webView:webView dragSourceActionMaskForPoint:point];
    return _private->dragSourceActionMask;
}

- (BOOL)_canSmartCopyOrDelete
{
    return [[self _webView] smartInsertDeleteEnabled] && [[self _bridge] selectionGranularity] == WebBridgeSelectByWord;
}

- (DOMRange *)_smartDeleteRangeForProposedRange:(DOMRange *)proposedRange
{
    if (proposedRange == nil || [self _canSmartCopyOrDelete] == NO)
        return nil;
    
    return [[self _bridge] smartDeleteRangeForProposedRange:proposedRange];
}

- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    if (pasteString == nil || rangeToReplace == nil || [[self _webView] smartInsertDeleteEnabled] == NO) {
        if (beforeString)
            *beforeString = nil;
        if (afterString)
            *afterString = nil;
        return;
    }
    
    [[self _bridge] smartInsertForString:pasteString replacingRange:rangeToReplace beforeString:beforeString afterString:afterString];
}

- (BOOL)_wasFirstResponderAtMouseDownTime:(NSResponder *)responder
{
    return responder == _private->firstResponderAtMouseDownTime;
}

- (void)_pauseNullEventsForAllNetscapePlugins
{
    NSArray *subviews = [self subviews];
    unsigned int subviewCount = [subviews count];
    unsigned int subviewIndex;
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) {
        NSView *subview = [subviews objectAtIndex:subviewIndex];
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]])
            [(WebBaseNetscapePluginView *)subview stopNullEvents];
    }
}

- (void)_resumeNullEventsForAllNetscapePlugins
{
    NSArray *subviews = [self subviews];
    unsigned int subviewCount = [subviews count];
    unsigned int subviewIndex;
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) {
        NSView *subview = [subviews objectAtIndex:subviewIndex];
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]])
            [(WebBaseNetscapePluginView *)subview restartNullEvents];
    }
}

@end

@implementation WebHTMLView (WebNSTextInputSupport)

static NSArray *validAttributes = nil;

- (NSArray *)validAttributesForMarkedText
{
    if (!validAttributes) {
        validAttributes = [[NSArray allocWithZone:[self zone]] initWithObjects:NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName, NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
        // NSText also supports the following attributes, but it's
        // hard to tell which are really required for text input to
        // work well; I have not seen any input method make use of them yet.
        //
        // NSFontAttributeName, NSForegroundColorAttributeName,
        // NSBackgroundColorAttributeName, NSLanguageAttributeName,
        // NSTextInputReplacementRangeAttributeName
    }

    return validAttributes;
}

- (unsigned int)characterIndexForPoint:(NSPoint)thePoint
{
    NSWindow *window = [self window];
    WebBridge *bridge = [self _bridge];

    if (window)
        thePoint = [window convertScreenToBase:thePoint];
    thePoint = [self convertPoint:thePoint fromView:nil];

    DOMRange *range = [bridge characterRangeAtPoint:thePoint];
    if (!range)
        return NSNotFound;
    
    return [bridge convertDOMRangeToNSRange:range].location;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{
    WebBridge *bridge = [self _bridge];
    
    DOMRange *range;
    
    if ([self hasMarkedText]) {
        range = [bridge convertNSRangeToDOMRange:theRange];
    }
    else {
        range = [self _selectedRange];
    }
    
    NSRect resultRect;
    if ([range startContainer]) {
        resultRect = [self convertRect:[bridge firstRectForDOMRange:range] toView:nil];
        resultRect.origin = [[self window] convertBaseToScreen:resultRect.origin];
    }
    else {
        resultRect = NSMakeRect(0,0,0,0);
    }
    return resultRect;
}

- (NSRange)selectedRange
{
    WebBridge *bridge = [self _bridge];
    
    NSRange range = [bridge selectedNSRange];

    return range;
}

- (NSRange)markedRange
{
    if (![self hasMarkedText]) {
        return NSMakeRange(NSNotFound,0);
    }

    NSRange range = [[self _bridge] markedTextNSRange];

    return range;
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)theRange
{
    ERROR("TEXTINPUT: attributedSubstringFromRange: not yet implemented");
    return nil;
}

// test for 10.4 because of <rdar://problem/4243463>
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
- (long)conversationIdentifier
{
    return (long)self;
}
#else
- (NSInt)conversationIdentifier
{
    return (NSInt)self;
}
#endif

- (BOOL)hasMarkedText
{
    return [[self _bridge] markedTextDOMRange] != nil;
}

- (void)unmarkText
{
    [[self _bridge] setMarkedTextDOMRange:nil customAttributes:nil ranges:nil];
}

- (void)_selectMarkedText
{
    if ([self hasMarkedText]) {
        WebBridge *bridge = [self _bridge];
        DOMRange *markedTextRange = [bridge markedTextDOMRange];
        [bridge setSelectedDOMRange:markedTextRange affinity:NSSelectionAffinityDownstream closeTyping:NO];
    }
}

- (void)_selectRangeInMarkedText:(NSRange)range
{
    ASSERT([self hasMarkedText]);

    WebBridge *bridge = [self _bridge];
    DOMRange *selectedRange = [[bridge DOMDocument] createRange];
    DOMRange *markedTextRange = [bridge markedTextDOMRange];
    
    ASSERT([markedTextRange startContainer] == [markedTextRange endContainer]);

    unsigned selectionStart = [markedTextRange startOffset] + range.location;
    unsigned selectionEnd = selectionStart + range.length;

    [selectedRange setStart:[markedTextRange startContainer] :selectionStart];
    [selectedRange setEnd:[markedTextRange startContainer] :selectionEnd];

    [bridge setSelectedDOMRange:selectedRange affinity:NSSelectionAffinityDownstream closeTyping:NO];
}

- (void)_extractAttributes:(NSArray **)a ranges:(NSArray **)r fromAttributedString:(NSAttributedString *)string
{
    int length = [[string string] length];
    int i = 0;
    NSMutableArray *attributes = [NSMutableArray array];
    NSMutableArray *ranges = [NSMutableArray array];
    while (i < length) {
        NSRange effectiveRange;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&effectiveRange inRange:NSMakeRange(i,length - i)];
        [attributes addObject:attrs];
        [ranges addObject:[NSValue valueWithRange:effectiveRange]];
        i = effectiveRange.location + effectiveRange.length;
    }
    *a = attributes;
    *r = ranges;
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    WebBridge *bridge = [self _bridge];

    if (![self _isEditable])
        return;

    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString

    if (isAttributedString) {
        unsigned markedTextLength = [(NSString *)string length];
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, markedTextLength)];
        // The AppKit adds a 'secret' property to the string that contains the replacement
        // range.  The replacement range is the range of the the text that should be replaced
        // with the new string.
        if (rangeString) {
            [[self _bridge] selectNSRange:NSRangeFromString(rangeString)];
        }
    }

    _private->ignoreMarkedTextSelectionChange = YES;

    // if we had marked text already, we need to make sure to replace
    // that, instead of the selection/caret
    [self _selectMarkedText];

    NSString *text;
    NSArray *attributes = nil;
    NSArray *ranges = nil;
    if (isAttributedString) {
        text = [string string];
        [self _extractAttributes:&attributes ranges:&ranges fromAttributedString:string];
    } else {
        text = string;
    }

    [bridge replaceMarkedTextWithText:text];
    [bridge setMarkedTextDOMRange:[self _selectedRange] customAttributes:attributes ranges:ranges];
    if ([self hasMarkedText]) {
        [self _selectRangeInMarkedText:newSelRange];
    }

    _private->ignoreMarkedTextSelectionChange = NO;
}

- (void)doCommandBySelector:(SEL)aSelector
{
    WebView *webView = [self _webView];
    if (![[webView _editingDelegateForwarder] webView:webView doCommandBySelector:aSelector]) {
        [super doCommandBySelector:aSelector];
    }
}

- (void)_discardMarkedText
{
    if (![self hasMarkedText])
        return;

    _private->ignoreMarkedTextSelectionChange = YES;

    [self _selectMarkedText];
    [self unmarkText];
    [[NSInputManager currentInputManager] markedTextAbandoned:self];
    // FIXME: Should we be calling the delegate here?
    [[self _bridge] deleteSelectionWithSmartDelete:NO];

    _private->ignoreMarkedTextSelectionChange = NO;
}

- (void)_insertText:(NSString *)text selectInsertedText:(BOOL)selectText
{
    if (text == nil || [text length] == 0 || (![self _isEditable] && ![self hasMarkedText])) {
        return;
    }

    if (![self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionTyped]) {
        [self _discardMarkedText];
        return;
    }

    _private->ignoreMarkedTextSelectionChange = YES;

    // If we had marked text, we replace that, instead of the selection/caret.
    [self _selectMarkedText];

    [[self _bridge] insertText:text selectInsertedText:selectText];

    _private->ignoreMarkedTextSelectionChange = NO;

    // Inserting unmarks any marked text.
    [self unmarkText];
}

- (void)insertText:(id)string
{
    NSString *text;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        text = [string string];
        // we don't yet support inserting an attributed string but input methods
        // don't appear to require this.
    } else {
        text = string;
    }

    [self _insertText:text selectInsertedText:NO];
}

- (BOOL)_selectionIsInsideMarkedText
{
    WebBridge *bridge = [self _bridge];
    DOMRange *selection = [self _selectedRange];
    DOMRange *markedTextRange = [bridge markedTextDOMRange];

    ASSERT([markedTextRange startContainer] == [markedTextRange endContainer]);

    if ([selection startContainer] != [markedTextRange startContainer]) 
        return NO;

    if ([selection endContainer] != [markedTextRange startContainer])
        return NO;

    if ([selection startOffset] < [markedTextRange startOffset])
        return NO;

    if ([selection endOffset] > [markedTextRange endOffset])
        return NO;

    return YES;
}

- (void)_updateSelectionForInputManager
{
    if (![self hasMarkedText] || _private->ignoreMarkedTextSelectionChange)
        return;

    if ([self _selectionIsInsideMarkedText]) {
        DOMRange *selection = [self _selectedRange];
        DOMRange *markedTextDOMRange = [[self _bridge] markedTextDOMRange];

        unsigned markedSelectionStart = [selection startOffset] - [markedTextDOMRange startOffset];
        unsigned markedSelectionLength = [selection endOffset] - [selection startOffset];
        NSRange newSelectionRange = NSMakeRange(markedSelectionStart, markedSelectionLength);

        [[NSInputManager currentInputManager] markedTextSelectionChanged:newSelectionRange client:self];
    } else {
        [self unmarkText];
        [[NSInputManager currentInputManager] markedTextAbandoned:self];
    }
}

@end

/*
    This class runs the show for handing the complete: NSTextView operation.  It counts on its HTML view
    to call endRevertingChange: whenever the current completion needs to be aborted.
 
    The class is in one of two modes:  PopupWindow showing, or not.  It is shown when a completion yields
    more than one match.  If a completion yields one or zero matches, it is not shown, and **there is no
    state carried across to the next completion**.
 */
@implementation WebTextCompleteController

- (id)initWithHTMLView:(WebHTMLView *)view
{
    self = [super init];
    if (!self)
        return nil;
    _view = view;
    return self;
}

- (void)dealloc
{
    [_popupWindow release];
    [_completions release];
    [_originalString release];
    [super dealloc];
}

- (void)_insertMatch:(NSString *)match
{
    // FIXME: 3769654 - We should preserve case of string being inserted, even in prefix (but then also be
    // able to revert that).  Mimic NSText.
    WebBridge *bridge = [_view _bridge];
    NSString *newText = [match substringFromIndex:prefixLength];
    [bridge replaceSelectionWithText:newText selectReplacement:YES smartReplace:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_buildUI
{
    NSRect scrollFrame = NSMakeRect(0, 0, 100, 100);
    NSRect tableFrame = NSZeroRect;    
    tableFrame.size = [NSScrollView contentSizeForFrameSize:scrollFrame.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
    // Added cast to work around problem with multiple Foundation initWithIdentifier: methods with different parameter types.
    NSTableColumn *column = [(NSTableColumn *)[NSTableColumn alloc] initWithIdentifier:[NSNumber numberWithInt:0]];
    [column setWidth:tableFrame.size.width];
    [column setEditable:NO];
    
    _tableView = [[NSTableView alloc] initWithFrame:tableFrame];
    [_tableView setAutoresizingMask:NSViewWidthSizable];
    [_tableView addTableColumn:column];
    [column release];
    [_tableView setDrawsGrid:NO];
    [_tableView setCornerView:nil];
    [_tableView setHeaderView:nil];
    [_tableView setAutoresizesAllColumnsToFit:YES];
    [_tableView setDelegate:self];
    [_tableView setDataSource:self];
    [_tableView setTarget:self];
    [_tableView setDoubleAction:@selector(tableAction:)];
    
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:scrollFrame];
    [scrollView setBorderType:NSNoBorder];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollView setDocumentView:_tableView];
    [_tableView release];
    
    _popupWindow = [[NSWindow alloc] initWithContentRect:scrollFrame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_popupWindow setAlphaValue:0.88];
    [_popupWindow setContentView:scrollView];
    [scrollView release];
    [_popupWindow setHasShadow:YES];
    [_popupWindow setOneShot:YES];
    //[_popupWindow _setForceActiveControls:YES];   // AK secret - no known problem from leaving this out
    [_popupWindow setReleasedWhenClosed:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_placePopupWindow:(NSPoint)topLeft
{
    int numberToShow = [_completions count];
    if (numberToShow > 20) {
        numberToShow = 20;
    }

    NSRect windowFrame;
    NSPoint wordStart = topLeft;
    windowFrame.origin = [[_view window] convertBaseToScreen:[_view convertPoint:wordStart toView:nil]];
    windowFrame.size.height = numberToShow * [_tableView rowHeight] + (numberToShow + 1) * [_tableView intercellSpacing].height;
    windowFrame.origin.y -= windowFrame.size.height;
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:12.0], NSFontAttributeName, nil];
    float maxWidth = 0.0;
    int maxIndex = -1;
    int i;
    for (i = 0; i < numberToShow; i++) {
        float width = ceil([[_completions objectAtIndex:i] sizeWithAttributes:attributes].width);
        if (width > maxWidth) {
            maxWidth = width;
            maxIndex = i;
        }
    }
    windowFrame.size.width = 100;
    if (maxIndex >= 0) {
        maxWidth = ceil([NSScrollView frameSizeForContentSize:NSMakeSize(maxWidth, 100) hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder].width);
        maxWidth = ceil([NSWindow frameRectForContentRect:NSMakeRect(0, 0, maxWidth, 100) styleMask:NSBorderlessWindowMask].size.width);
        maxWidth += 5.0;
        windowFrame.size.width = MAX(maxWidth, windowFrame.size.width);
        maxWidth = MIN(400.0, windowFrame.size.width);
    }
    [_popupWindow setFrame:windowFrame display:NO];
    
    [_tableView reloadData];
    [_tableView selectRow:0 byExtendingSelection:NO];
    [_tableView scrollRowToVisible:0];
    [self _reflectSelection];
    [_popupWindow setLevel:NSPopUpMenuWindowLevel];
    [_popupWindow orderFront:nil];    
    [[_view window] addChildWindow:_popupWindow ordered:NSWindowAbove];
}

- (void)doCompletion
{
    if (!_popupWindow) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker) {
            ERROR("No NSSpellChecker");
            return;
        }

        // Get preceeding word stem
        WebBridge *bridge = [_view _bridge];
        DOMRange *selection = [bridge selectedDOMRange];
        DOMRange *wholeWord = [bridge rangeByAlteringCurrentSelection:WebSelectByExtending direction:WebBridgeSelectBackward granularity:WebBridgeSelectByWord];
        DOMRange *prefix = [wholeWord cloneRange];
        [prefix setEnd:[selection startContainer] :[selection startOffset]];

        // Reject some NOP cases
        if ([prefix collapsed]) {
            NSBeep();
            return;
        }
        NSString *prefixStr = [bridge stringForRange:prefix];
        NSString *trimmedPrefix = [prefixStr stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([trimmedPrefix length] == 0) {
            NSBeep();
            return;
        }
        prefixLength = [prefixStr length];

        // Lookup matches
        [_completions release];
        _completions = [checker completionsForPartialWordRange:NSMakeRange(0, [prefixStr length]) inString:prefixStr language:nil inSpellDocumentWithTag:[[_view _webView] spellCheckerDocumentTag]];
        [_completions retain];
    
        if (!_completions || [_completions count] == 0) {
            NSBeep();
        } else if ([_completions count] == 1) {
            [self _insertMatch:[_completions objectAtIndex:0]];
        } else {
            ASSERT(!_originalString);       // this should only be set IFF we have a popup window
            _originalString = [[bridge stringForRange:selection] retain];
            [self _buildUI];
            NSRect wordRect = [bridge caretRectAtNode:[wholeWord startContainer] offset:[wholeWord startOffset] affinity:NSSelectionAffinityDownstream];
            // +1 to be under the word, not the caret
            // FIXME - 3769652 - Wrong positioning for right to left languages.  We should line up the upper
            // right corner with the caret instead of upper left, and the +1 would be a -1.
            NSPoint wordLowerLeft = { NSMinX(wordRect)+1, NSMaxY(wordRect) };
            [self _placePopupWindow:wordLowerLeft];
        }
    } else {
        [self endRevertingChange:YES moveLeft:NO];
    }
}

- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft
{
    if (_popupWindow) {
        // tear down UI
        [[_view window] removeChildWindow:_popupWindow];
        [_popupWindow orderOut:self];
        // Must autorelease because event tracking code may be on the stack touching UI
        [_popupWindow autorelease];
        _popupWindow = nil;

        if (revertChange) {
            WebBridge *bridge = [_view _bridge];
            [bridge replaceSelectionWithText:_originalString selectReplacement:YES smartReplace:NO];
        } else if (goLeft) {
            [_view moveBackward:nil];
        } else {
            [_view moveForward:nil];
        }
        [_originalString release];
        _originalString = nil;
    }
    // else there is no state to abort if the window was not up
}

// WebHTMLView gives us a crack at key events it sees.  Return whether we consumed the event.
// The features for the various keys mimic NSTextView.
- (BOOL)filterKeyDown:(NSEvent *)event
{
    if (_popupWindow) {
        NSString *string = [event charactersIgnoringModifiers];
        unichar c = [string characterAtIndex:0];
        if (c == NSUpArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (0 < selectedRow) {
                [_tableView selectRow:selectedRow-1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow-1];
            }
            return YES;
        } else if (c == NSDownArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (selectedRow < (int)[_completions count]-1) {
                [_tableView selectRow:selectedRow+1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow+1];
            }
            return YES;
        } else if (c == NSRightArrowFunctionKey || c == '\n' || c == '\r' || c == '\t') {
            [self endRevertingChange:NO moveLeft:NO];
            return YES;
        } else if (c == NSLeftArrowFunctionKey) {
            [self endRevertingChange:NO moveLeft:YES];
            return YES;
        } else if (c == 0x1b || c == NSF5FunctionKey) {
            [self endRevertingChange:YES moveLeft:NO];
            return YES;
        } else if (c == ' ' || ispunct(c)) {
            [self endRevertingChange:NO moveLeft:NO];
            return NO;  // let the char get inserted
        }
    }
    return NO;
}

- (void)_reflectSelection
{
    int selectedRow = [_tableView selectedRow];
    ASSERT(selectedRow >= 0 && selectedRow < (int)[_completions count]);
    [self _insertMatch:[_completions objectAtIndex:selectedRow]];
}

- (void)tableAction:(id)sender
{
    [self _reflectSelection];
    [self endRevertingChange:NO moveLeft:NO];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [_completions count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    return [_completions objectAtIndex:row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [self _reflectSelection];
}

@end
