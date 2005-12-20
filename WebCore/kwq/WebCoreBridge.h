/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <Cocoa/Cocoa.h>

#import <JavaScriptCore/npruntime.h>
#import <JavaVM/jni.h>
#import <WebCore/WebCoreKeyboardAccess.h>
#import <WebKit/WebView.h>

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

@class DOMCSSStyleDeclaration;
@class DOMDocument;
@class DOMDocumentFragment;
@class DOMElement;
@class DOMHTMLElement;
@class DOMHTMLInputElement;
@class DOMNode;
@class DOMRange;
@class DOMHTMLTextAreaElement;
@class WebCoreSettings;
@class WebScriptObject;

@protocol WebCoreDOMTreeCopier;
@protocol WebCoreRenderTreeCopier;
@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;
@protocol WebCoreFileButton;
@protocol WebCoreFileButtonDelegate;

extern NSString *WebCoreElementDOMNodeKey;
extern NSString *WebCoreElementFrameKey;
extern NSString *WebCoreElementImageAltStringKey;
extern NSString *WebCoreElementImageRendererKey;
extern NSString *WebCoreElementImageRectKey;
extern NSString *WebCoreElementImageURLKey;
extern NSString *WebCoreElementIsSelectedKey;
extern NSString *WebCoreElementLinkURLKey;
extern NSString *WebCoreElementLinkTargetFrameKey;
extern NSString *WebCoreElementLinkLabelKey;
extern NSString *WebCoreElementLinkTitleKey;
extern NSString *WebCoreElementTitleKey;

extern NSString *WebCorePageCacheStateKey;

typedef enum {
    WebCoreDeviceScreen,
    WebCoreDevicePrinter
} WebCoreDeviceType;

typedef enum {
    WebSelectionStateNone,
    WebSelectionStateCaret,
    WebSelectionStateRange,
} WebSelectionState;

typedef enum {
    WebSelectByMoving,
    WebSelectByExtending
} WebSelectionAlteration;

typedef enum {
    WebBridgeSelectForward,
    WebBridgeSelectBackward,
    WebBridgeSelectRight,
    WebBridgeSelectLeft
} WebBridgeSelectionDirection;

typedef enum {
    WebBridgeSelectByCharacter,
    WebBridgeSelectByWord,
    WebBridgeSelectByLine,
    WebBridgeSelectByParagraph,
    WebBridgeSelectToLineBoundary,
    WebBridgeSelectToParagraphBoundary,
    WebBridgeSelectToDocumentBoundary
} WebBridgeSelectionGranularity;

typedef enum {
    WebScrollUp,
    WebScrollDown,
    WebScrollLeft,
    WebScrollRight
} WebScrollDirection;

typedef enum {
    WebScrollLine,
    WebScrollPage,
    WebScrollDocument,
    WebScrollWheel
} WebScrollGranularity;

typedef enum {
    WebUndoActionUnspecified,
    WebUndoActionSetColor,
    WebUndoActionSetBackgroundColor,
    WebUndoActionTurnOffKerning,
    WebUndoActionTightenKerning,
    WebUndoActionLoosenKerning,
    WebUndoActionUseStandardKerning,
    WebUndoActionTurnOffLigatures,
    WebUndoActionUseStandardLigatures,
    WebUndoActionUseAllLigatures,
    WebUndoActionRaiseBaseline,
    WebUndoActionLowerBaseline,
    WebUndoActionSetTraditionalCharacterShape,
    WebUndoActionSetFont,
    WebUndoActionChangeAttributes,
    WebUndoActionAlignLeft,
    WebUndoActionAlignRight,
    WebUndoActionCenter,
    WebUndoActionJustify,
    WebUndoActionSetWritingDirection,
    WebUndoActionSubscript,
    WebUndoActionSuperscript,
    WebUndoActionUnderline,
    WebUndoActionOutline,
    WebUndoActionUnscript,
    WebUndoActionDrag,
    WebUndoActionCut,
    WebUndoActionPaste,
    WebUndoActionPasteFont,
    WebUndoActionPasteRuler,
    WebUndoActionTyping,
} WebUndoAction;

typedef enum
{
    ObjectElementNone,
    ObjectElementImage,
    ObjectElementFrame,
    ObjectElementPlugin,
} ObjectElementType;

// WebCoreBridge objects are used by WebCore to abstract away operations that need
// to be implemented by library clients, for example WebKit. The objects are also
// used in the opposite direction, for simple access to WebCore functions without dealing
// directly with the KHTML C++ classes.

// A WebCoreBridge creates and holds a reference to a KHTMLPart.

// The WebCoreBridge interface contains methods for use by the non-WebCore side of the bridge.

@interface WebCoreBridge : NSObject
{
    KWQKHTMLPart *_part;
    KHTMLRenderPart *_renderPart;
    RenderArena *_renderPartArena;
    BOOL _shouldCreateRenderers;
}

+ (WebCoreBridge *)bridgeForDOMDocument:(DOMDocument *)document;

+ (NSArray *)supportedMIMETypes;

- (void)initializeSettings:(WebCoreSettings *)settings;

- (void)setRenderPart:(KHTMLRenderPart *)renderPart;
- (KHTMLRenderPart *)renderPart;

- (void)setName:(NSString *)name;
- (NSString *)name;

- (KWQKHTMLPart *)part;

- (void)setParent:(WebCoreBridge *)parent;

- (void)provisionalLoadStarted;

- (void)openURL:(NSURL *)URL reload:(BOOL)reload
    contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified
    pageCache:(NSDictionary *)pageCache;
- (void)setEncoding:(NSString *)encoding userChosen:(BOOL)userChosen;
- (void)addData:(NSData *)data;
- (void)closeURL;
- (void)stopLoading;

- (void)didNotOpenURL:(NSURL *)URL pageCache:(NSDictionary *)pageCache;

- (BOOL)canLoadURL:(NSURL *)URL fromReferrer:(NSString *)referrer hideReferrer:(BOOL *)hideReferrer;

- (void)saveDocumentState;
- (void)restoreDocumentState;

- (BOOL)canCachePage;
- (BOOL)saveDocumentToPageCache;

- (void)end;
- (void)stop;

- (void)handleFallbackContent;

- (NSURL *)URL;
- (NSURL *)baseURL;
- (NSString *)referrer;
- (NSString *)domain;
- (WebCoreBridge *)opener;
- (void)setOpener:(WebCoreBridge *)bridge;

- (void)installInFrame:(NSView *)view;
- (void)removeFromFrame;

- (void)scrollToAnchor:(NSString *)anchor;
- (void)scrollToAnchorWithURL:(NSURL *)URL;

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity;

- (void)createKHTMLViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh;

- (BOOL)isFrameSet;

- (void)reapplyStylesForDeviceType:(WebCoreDeviceType)deviceType;
- (void)forceLayoutAdjustingViewSize:(BOOL)adjustSizeFlag;
- (void)forceLayoutWithMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustSizeFlag;
- (void)sendResizeEvent;
- (void)sendScrollEvent;
- (BOOL)needsLayout;
- (void)setNeedsLayout;
- (void)drawRect:(NSRect)rect;
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit;
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight;

- (void)setActivationEventNumber:(int)num;
- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseMoved:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;

// these return YES if event is eaten by WebCore
- (BOOL)sendScrollWheelEvent:(NSEvent *)event;
- (BOOL)sendContextMenuEvent:(NSEvent *)event;

- (NSView *)nextKeyView;
- (NSView *)previousKeyView;

- (NSView *)nextKeyViewInsideWebFrameViews;
- (NSView *)previousKeyViewInsideWebFrameViews;

- (NSObject *)copyDOMTree:(id <WebCoreDOMTreeCopier>)copier;
- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier;
- (NSString *)renderTreeAsExternalRepresentation;

- (NSDictionary *)elementAtPoint:(NSPoint)point;
- (NSURL *)URLWithAttributeString:(NSString *)string;

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form;
- (DOMElement *)elementForView:(NSView *)view;
- (BOOL)elementDoesAutoComplete:(DOMElement *)element;
- (BOOL)elementIsPassword:(DOMElement *)element;
- (DOMElement *)formForElement:(DOMElement *)element;
- (DOMElement *)currentForm;
- (NSArray *)controlsInForm:(DOMElement *)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element;

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;

- (NSString *)advanceToNextMisspelling;
- (NSString *)advanceToNextMisspellingStartingJustBeforeSelection;
- (void)unmarkAllMisspellings;

- (void)setTextSizeMultiplier:(float)multiplier;

- (CFStringEncoding)textEncoding;

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;

- (DOMDocument *)DOMDocument;
- (DOMHTMLElement *)frameElement;

- (BOOL)isSelectionEditable;
- (WebSelectionState)selectionState;

- (NSAttributedString *)selectedAttributedString;
- (NSString *)selectedString;

- (void)setSelectionFromNone;
- (void)setDisplaysWithFocusAttributes:(BOOL)flag;

- (void)setWindowHasFocus:(BOOL)flag;

- (NSString *)stringForRange:(DOMRange *)range;

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes;
- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes;

- (void)selectAll;
- (void)deselectAll;
- (void)deselectText;

- (NSRect)selectionRect;
- (NSRect)visibleSelectionRect;
- (void)centerSelectionInVisibleArea;
- (NSImage *)selectionImage;
- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity;
- (NSRect)firstRectForDOMRange:(DOMRange *)range;

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity closeTyping:(BOOL)closeTyping;
- (DOMRange *)selectedDOMRange;
- (NSSelectionAffinity)selectionAffinity;

// Emacs-style-editing "mark"
- (void)setMarkDOMRange:(DOMRange *)range;
- (DOMRange *)markDOMRange;

// international text input "marked text"
- (void)setMarkedTextDOMRange:(DOMRange *)range customAttributes:(NSArray *)attributes ranges:(NSArray *)ranges;
- (DOMRange *)markedTextDOMRange;
- (void)replaceMarkedTextWithText:(NSString *)text;

- (NSAttributedString *)attributedStringFrom:(DOMNode *)startNode startOffset:(int)startOffset to:(DOMNode *)endNode endOffset:(int)endOffset;

- (NSFont *)fontForSelection:(BOOL *)hasMultipleFonts;
- (NSDictionary *)fontAttributesForSelectionStart;
- (NSWritingDirection)baseWritingDirectionForSelectionStart;

+ (NSString *)stringWithData:(NSData *)data textEncoding:(CFStringEncoding)textEncoding;
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName;

- (BOOL)interceptKeyEvent:(NSEvent *)event toView:(NSView *)view;

- (void)setShouldCreateRenderers:(BOOL)f;
- (BOOL)shouldCreateRenderers;

- (int)numPendingOrLoadingRequests;
- (BOOL)doneProcessingData;
- (BOOL)shouldClose;

- (void)setDrawsBackground:(BOOL)drawsBackround;

- (NSColor *)bodyBackgroundColor;
- (NSColor *)selectionColor;

- (void)adjustViewSize;

- (id)accessibilityTree;

- (void)undoEditing:(id)arg;
- (void)redoEditing:(id)arg;

- (DOMRange *)rangeByExpandingSelectionWithGranularity:(WebBridgeSelectionGranularity)granularity;
- (DOMRange *)rangeOfCharactersAroundCaret;
- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity;
- (void)alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity;
- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)distance;
- (void)alterCurrentSelection:(WebSelectionAlteration)alteration verticalDistance:(float)distance;
- (WebBridgeSelectionGranularity)selectionGranularity;
- (DOMRange *)smartDeleteRangeForProposedRange:(DOMRange *)proposedCharRange;
- (void)smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)charRangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString;
- (BOOL)canDeleteRange:(DOMRange *)range;
- (void)selectNSRange:(NSRange)range;
- (NSRange)selectedNSRange;
- (NSRange)markedTextNSRange;
- (DOMRange *)convertNSRangeToDOMRange:(NSRange)range;
- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range;

- (DOMDocumentFragment *)documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text;
- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes;

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

- (void)insertLineBreak;
- (void)insertParagraphSeparator;
- (void)insertParagraphSeparatorInQuotedContent;
- (void)insertText:(NSString *)text selectInsertedText:(BOOL)selectInsertedText;

- (void)setSelectionToDragCaret;
- (void)moveSelectionToDragCaret:(DOMDocumentFragment *)selectionFragment smartMove:(BOOL)smartMove;
- (void)moveDragCaretToPoint:(NSPoint)point;
- (void)removeDragCaret;
- (DOMRange *)dragCaretDOMRange;
- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point;
- (DOMRange *)characterRangeAtPoint:(NSPoint)point;

- (void)deleteSelectionWithSmartDelete:(BOOL)smartDelete;
- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete;
- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete;

- (DOMCSSStyleDeclaration *)typingStyle;
- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (void)applyStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (void)applyParagraphStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (BOOL)selectionStartHasStyle:(DOMCSSStyleDeclaration *)style;
- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style;
- (void)applyEditingStyleToBodyElement;
- (void)removeEditingStyleFromBodyElement;
- (void)applyEditingStyleToElement:(DOMElement *)element;
- (void)removeEditingStyleFromElement:(DOMElement *)element;

- (void)ensureSelectionVisible;

- (WebScriptObject *)windowScriptObject;
- (NPObject *)windowScriptNPObject;

- (BOOL)eventMayStartDrag:(NSEvent *)event;
- (NSDragOperation)dragOperationForDraggingInfo:(id <NSDraggingInfo>)info;
- (void)dragExitedWithDraggingInfo:(id <NSDraggingInfo>)info;
- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)info;
- (void)dragSourceMovedTo:(NSPoint)windowLoc;
- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation;

- (BOOL)mayDHTMLCut;
- (BOOL)mayDHTMLCopy;
- (BOOL)mayDHTMLPaste;
- (BOOL)tryDHTMLCut;
- (BOOL)tryDHTMLCopy;
- (BOOL)tryDHTMLPaste;

- (NSMutableDictionary *)dashboardRegions;

- (void)clear;

@end

// The WebCoreBridge protocol contains methods for use by the WebCore side of the bridge.

// In NSArray objects for post data, NSData objects represent literal data, and NSString objects represent encoded files.
// The encoding is the standard form encoding for uploading files.

@protocol WebCoreBridge

- (NSArray *)childFrames; // WebCoreBridge objects
- (WebCoreBridge *)mainFrame;
- (WebCoreBridge *)findFrameNamed:(NSString *)name;
/* Creates a name for an frame unnamed in the HTML.  It should produce repeatable results for loads of the same frameset. */
- (NSString *)generateFrameName;
- (void)frameDetached;
- (NSView *)documentView;
- (WebView *)webView;

- (void)loadURL:(NSURL *)URL referrer:(NSString *)referrer reload:(BOOL)reload userGesture:(BOOL)forUser target:(NSString *)target triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;
- (void)postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;

- (WebCoreBridge *)createWindowWithURL:(NSURL *)URL frameName:(NSString *)name;
- (void)showWindow;

- (BOOL)canRunModal;
- (BOOL)canRunModalNow;
- (WebCoreBridge *)createModalDialogWithURL:(NSURL *)URL;
- (void)runModal;

- (NSString *)userAgentForURL:(NSURL *)URL;

- (void)setTitle:(NSString *)title;
- (void)setStatusText:(NSString *)status;

- (void)setIconURL:(NSURL *)URL;
- (void)setIconURL:(NSURL *)URL withType:(NSString *)string;

- (WebCoreBridge *)createChildFrameNamed:(NSString *)frameName withURL:(NSURL *)URL
    referrer:(NSString *)referrer
    renderPart:(KHTMLRenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (BOOL)areToolbarsVisible;
- (void)setToolbarsVisible:(BOOL)visible;
- (BOOL)isStatusbarVisible;
- (void)setStatusbarVisible:(BOOL)visible;
- (BOOL)areScrollbarsVisible;
- (void)setScrollbarsVisible:(BOOL)visible;
- (NSWindow *)window;
- (void)setWindowFrame:(NSRect)frame;
- (NSRect)windowFrame;
- (void)setWindowContentRect:(NSRect)frame;
- (NSRect)windowContentRect;

- (void)setWindowIsResizable:(BOOL)resizable;
- (BOOL)windowIsResizable;

- (NSResponder *)firstResponder;
- (void)makeFirstResponder:(NSResponder *)responder;

- (BOOL)wasFirstResponderAtMouseDownTime:(NSResponder *)responder;

- (void)closeWindowSoon;

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message;
- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message;
- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result;
- (BOOL)canRunBeforeUnloadConfirmPanel;
- (BOOL)runBeforeUnloadConfirmPanelWithMessage:(NSString *)message;
- (void)addMessageToConsole:(NSDictionary *)message;

- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)loader withURL:(NSURL *)URL customHeaders:(NSDictionary *)customHeaders;
- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)loader withURL:(NSURL *)URL customHeaders:(NSDictionary *)customHeaders postData:(NSArray *)data;
- (void)objectLoadedFromCacheWithURL:(NSURL *)URL response:(NSURLResponse *)response data:(NSData *)data;

- (NSData *)syncLoadResourceWithURL:(NSURL *)URL customHeaders:(NSDictionary *)requestHeaders postData:(NSArray *)postData finalURL:(NSURL **)finalNSURL responseHeaders:(NSDictionary **)responseHeaderDict statusCode:(int *)statusCode;

- (BOOL)isReloading;
- (time_t)expiresTimeForResponse:(NSURLResponse *)response;

- (void)reportClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction;
- (void)reportClientRedirectCancelled:(BOOL)cancelWithLoadInProgress;

- (void)focusWindow;
- (void)unfocusWindow;

- (void)formControlIsResigningFirstResponder:(NSView *)formControl;

- (NSView *)nextKeyViewOutsideWebFrameViews;
- (NSView *)nextValidKeyViewOutsideWebFrameViews;
- (NSView *)previousKeyViewOutsideWebFrameViews;

- (BOOL)defersLoading;
- (void)setDefersLoading:(BOOL)loading;
- (void)saveDocumentState:(NSArray *)documentState;
- (NSArray *)documentState;

- (void)setNeedsReapplyStyles;

- (void)tokenizerProcessedData;

// OK to be an NSString rather than an NSURL.
// This URL is only used for coloring visited links.
- (NSString *)requestedURLString;
- (NSString *)incomingReferrer;

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                  attributeNames:(NSArray *)attributeNames
                 attributeValues:(NSArray *)attributeValues
                        MIMEType:(NSString *)MIMEType;
- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame
                        attributeNames:(NSArray *)attributeNames
                       attributeValues:(NSArray *)attributeValues
                               baseURL:(NSURL *)baseURL;

- (BOOL)saveDocumentToPageCache:(id)documentInfo;

- (int)getObjectCacheSize;

- (ObjectElementType)determineObjectFromMIMEType:(NSString*)MIMEType URL:(NSURL*)URL;

- (void)loadEmptyDocumentSynchronously;

- (NSString *)MIMETypeForPath:(NSString *)path;

- (void)allowDHTMLDrag:(BOOL *)flagDHTML UADrag:(BOOL *)flagUA;
- (BOOL)startDraggingImage:(NSImage *)dragImage at:(NSPoint)dragLoc operation:(NSDragOperation)op event:(NSEvent *)event sourceIsDHTML:(BOOL)flag DHTMLWroteData:(BOOL)dhtmlWroteData;
- (void)handleAutoscrollForMouseDragged:(NSEvent *)event;
- (BOOL)mayStartDragAtEventLocation:(NSPoint)location;

- (BOOL)selectWordBeforeMenuEvent;

- (int)historyLength;
- (void)goBackOrForward:(int)distance;
- (BOOL)canGoBackOrForward:(int)distance;

- (void)textFieldDidBeginEditing:(DOMHTMLInputElement *)element;
- (void)textFieldDidEndEditing:(DOMHTMLInputElement *)element;
- (void)textDidChangeInTextField:(DOMHTMLInputElement *)element;
- (void)textDidChangeInTextArea:(DOMHTMLTextAreaElement *)element;

- (BOOL)textField:(DOMHTMLInputElement *)element doCommandBySelector:(SEL)commandSelector;
- (BOOL)textField:(DOMHTMLInputElement *)element shouldHandleEvent:(NSEvent *)event;

- (NSView <WebCoreFileButton> *)fileButtonWithDelegate:(id <WebCoreFileButtonDelegate>)delegate;

- (void)setHasBorder:(BOOL)hasBorder;

- (WebCoreKeyboardUIMode)keyboardUIMode;

- (void)didSetName:(NSString *)name;

- (NSFileWrapper *)fileWrapperForURL:(NSURL *)URL;

- (void)print;

- (jobject)getAppletInView:(NSView *)view;

// Deprecated, use getAppletInView: instead.
- (jobject)pollForAppletInView:(NSView *)view;

- (NSUndoManager *)undoManager;
- (NSString *)nameForUndoAction:(WebUndoAction)undoAction;
- (void)issueCutCommand;
- (void)issueCopyCommand;
- (void)issuePasteCommand;
- (void)issuePasteAndMatchStyleCommand;
- (void)issueTransposeCommand;
- (void)respondToChangedSelection;
- (void)respondToChangedContents;
- (void)setIsSelected:(BOOL)isSelected forView:(NSView *)view;
- (BOOL)isEditable;
- (BOOL)shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
- (BOOL)shouldBeginEditing:(DOMRange *)range;
- (BOOL)shouldEndEditing:(DOMRange *)range;
- (BOOL)canPaste;

- (NSString *)overrideMediaType;

- (void)windowObjectCleared;

- (int)spellCheckerDocumentTag;
- (BOOL)isContinuousSpellCheckingEnabled;

- (void)didFirstLayout;

- (void)dashboardRegionsChanged:(NSMutableDictionary *)regions;

- (BOOL)isCharacterSmartReplaceExempt:(unichar)c isPreviousCharacter:(BOOL)isPreviousCharacter;

- (void)handledOnloadEvents;

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

@protocol WebCoreFileButton <NSObject>
- (void)setFilename:(NSString *)filename;
- (void)performClick;
- (NSString *)filename;
- (float)baseline;
- (void)setVisualFrame:(NSRect)rect;
- (NSRect)visualFrame;
- (NSSize)bestVisualFrameSizeForCharacterCount:(int)count;
@end

@protocol WebCoreFileButtonDelegate <NSObject>
- (void)filenameChanged:(NSString *)filename;
- (void)focusChanged:(BOOL)nowHasFocus;
- (void)clicked;
@end
