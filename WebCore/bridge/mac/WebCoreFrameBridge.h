/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifdef __cplusplus

namespace WebCore {
    class EditorClient;
    class Element;
    class FrameMac;
    class String;
}

typedef WebCore::EditorClient WebCoreEditorClient;
typedef WebCore::Element WebCoreElement;
typedef WebCore::FrameMac WebCoreFrameMac;

#else

@class NSMenu;
@class WebCoreEditorClient;
@class WebCoreElement;
@class WebCoreFrameMac;

#endif

@class DOMCSSStyleDeclaration;
@class DOMDocument;
@class DOMDocumentFragment;
@class DOMElement;
@class DOMHTMLElement;
@class DOMHTMLInputElement;
@class DOMHTMLTextAreaElement;
@class DOMNode;
@class DOMRange;
@class NSMenu;
@class WebCorePageBridge;
@class WebCoreSettings;
@class WebFrame;
@class WebScriptObject;
@class WebView;

@protocol WebCoreRenderTreeCopier;
@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

extern NSString *WebCorePageCacheStateKey;

typedef enum {
    WebCoreDeviceScreen,
    WebCoreDevicePrinter
} WebCoreDeviceType;

typedef enum {
    WebSelectionStateNone,
    WebSelectionStateCaret,
    WebSelectionStateRange
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
    WebBridgeSelectBySentence,
    WebBridgeSelectByLine,
    WebBridgeSelectByParagraph,
    WebBridgeSelectToSentenceBoundary,
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
    WebUndoActionCreateLink,
    WebUndoActionUnlink,
    WebUndoActionFormatBlock,
    WebUndoActionInsertList,
    WebUndoActionIndent,
    WebUndoActionOutdent
} WebUndoAction;

typedef enum {
    ObjectElementNone,
    ObjectElementImage,
    ObjectElementFrame,
    ObjectElementPlugin
} ObjectElementType;

@protocol WebCoreOpenPanelResultListener <NSObject>
- (void)chooseFilename:(NSString *)fileName;
- (void)cancel;
@end

// WebCoreFrameBridge objects are used by WebCore to abstract away operations that need
// to be implemented by library clients, for example WebKit. The objects are also
// used in the opposite direction, for simple access to WebCore functions without dealing
// directly with the KHTML C++ classes.

// A WebCoreFrameBridge creates and holds a reference to a Frame.

// The WebCoreFrameBridge interface contains methods for use by the non-WebCore side of the bridge.

@interface WebCoreFrameBridge : NSObject
{
    WebCoreFrameMac* m_frame;

    BOOL _shouldCreateRenderers;
    BOOL _closed;
}

- (WebCoreFrameMac*)_frame; // underscore to prevent conflict with -[NSView frame]

+ (WebCoreFrameBridge *)bridgeForDOMDocument:(DOMDocument *)document;

- (id)initMainFrameWithPage:(WebCorePageBridge *)page withEditorClient:(WebCoreEditorClient *)client;
- (id)initSubframeWithOwnerElement:(WebCoreElement *)ownerElement withEditorClient:(WebCoreEditorClient *)client;

- (void)close;

+ (NSArray *)supportedNonImageMIMETypes;
+ (NSArray *)supportedImageMIMETypes;
+ (NSArray *)supportedImageResourceMIMETypes; // includes types like PDF

- (void)openURL:(NSURL *)URL reload:(BOOL)reload
    contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified
    pageCache:(NSDictionary *)pageCache;
- (void)addData:(NSData *)data;
- (void)closeURL;

- (void)invalidatePageCache:(NSDictionary *)pageCache;

- (BOOL)canTargetLoadInFrame:(WebCoreFrameBridge *)targetFrame;

- (void)saveDocumentState;
- (void)restoreDocumentState;

- (BOOL)saveDocumentToPageCache;

- (void)clearFrame;

- (NSURL *)baseURL;

- (void)installInFrame:(NSView *)view;
- (void)removeFromFrame;

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity;

- (void)createFrameViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh;

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

- (NSView *)nextKeyView;
- (NSView *)previousKeyView;

- (NSView *)nextKeyViewInsideWebFrameViews;
- (NSView *)previousKeyViewInsideWebFrameViews;

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier;
- (NSString *)renderTreeAsExternalRepresentation;

- (void)getInnerNonSharedNode:(DOMNode **)innerNonSharedNode innerNode:(DOMNode **)innerNode URLElement:(DOMElement **)URLElement atPoint:(NSPoint)point allowShadowContent:(BOOL)allow;
- (BOOL)isPointInsideSelection:(NSPoint)point;

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
- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(unsigned)limit;
- (BOOL)markedTextMatchesAreHighlighted;
- (void)setMarkedTextMatchesAreHighlighted:(BOOL)doHighlight;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

- (void)setTextSizeMultiplier:(float)multiplier;

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string;

- (BOOL)mayCopy;

- (NSAttributedString *)selectedAttributedString;
- (NSString *)selectedString;

- (NSString *)stringForRange:(DOMRange *)range;

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes;
- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes;

- (void)selectAll;
- (void)deselectAll;
- (void)deselectText;

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity;
- (NSRect)firstRectForDOMRange:(DOMRange *)range;
- (void)scrollDOMRangeToVisible:(DOMRange *)range;

- (void)setSelectedDOMRange:(DOMRange *)range affinity:(NSSelectionAffinity)selectionAffinity closeTyping:(BOOL)closeTyping;
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
- (NSWritingDirection)baseWritingDirectionForSelectionStart;

- (NSString *)stringWithData:(NSData *)data; // using the encoding of the frame's main resource
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName; // nil for textEncodingName means Latin-1

- (void)setShouldCreateRenderers:(BOOL)f;
- (BOOL)shouldCreateRenderers;

- (int)numPendingOrLoadingRequests;
- (BOOL)doneProcessingData;

- (void)setBaseBackgroundColor:(NSColor *)backgroundColor;
- (void)setDrawsBackground:(BOOL)drawsBackround;

- (NSColor *)selectionColor;

- (id)accessibilityTree;

- (void)undoEditing:(id)arg;
- (void)redoEditing:(id)arg;

- (DOMRange *)rangeByExpandingSelectionWithGranularity:(WebBridgeSelectionGranularity)granularity;
- (DOMRange *)rangeOfCharactersAroundCaret;
- (DOMRange *)rangeByAlteringCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity;
- (void)alterCurrentSelection:(WebSelectionAlteration)alteration direction:(WebBridgeSelectionDirection)direction granularity:(WebBridgeSelectionGranularity)granularity;
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
- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text inContext:(DOMRange *)context;
- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes;

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

- (bool)canIncreaseSelectionListLevel;
- (bool)canDecreaseSelectionListLevel;
- (DOMNode *)increaseSelectionListLevel;
- (DOMNode *)increaseSelectionListLevelOrdered;
- (DOMNode *)increaseSelectionListLevelUnordered;
- (void)decreaseSelectionListLevel;

- (void)insertLineBreak;
- (void)insertParagraphSeparator;
- (void)insertParagraphSeparatorInQuotedContent;
- (void)insertText:(NSString *)text selectInsertedText:(BOOL)selectInsertedText;

- (void)setSelectionToDragCaret;
- (void)moveSelectionToDragCaret:(DOMDocumentFragment *)selectionFragment smartMove:(BOOL)smartMove;
- (void)moveDragCaretToPoint:(NSPoint)point;
- (DOMRange *)dragCaretDOMRange;
- (BOOL)isDragCaretRichlyEditable;
- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point;
- (DOMRange *)characterRangeAtPoint:(NSPoint)point;

- (void)deleteSelectionWithSmartDelete:(BOOL)smartDelete;
- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebBridgeSelectionGranularity)granularity;
- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebBridgeSelectionGranularity)granularity;

- (DOMCSSStyleDeclaration *)typingStyle;
- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (void)applyStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (void)applyParagraphStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebUndoAction)undoAction;
- (BOOL)selectionStartHasStyle:(DOMCSSStyleDeclaration *)style;
- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style;

- (NSDragOperation)dragOperationForDraggingInfo:(id <NSDraggingInfo>)info;
- (void)dragExitedWithDraggingInfo:(id <NSDraggingInfo>)info;
- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)info;
- (void)dragSourceMovedTo:(NSPoint)windowLoc;
- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation;

- (BOOL)isCharacterSmartReplaceExempt:(unichar)c isPreviousCharacter:(BOOL)isPreviousCharacter;

- (BOOL)getData:(NSData **)data andResponse:(NSURLResponse **)response forURL:(NSURL *)URL;
- (void)getAllResourceDatas:(NSArray **)datas andResponses:(NSArray **)responses;

- (BOOL)canProvideDocumentSource;
- (BOOL)canSaveAsWebArchive;

- (NSURL*)originalRequestURL;
- (void)frameDetached;
- (void)tokenizerProcessedData;
- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName;
- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)loader withMethod:(NSString *)method URL:(NSURL *)URL customHeaders:(NSDictionary *)customHeaders;
- (id <WebCoreResourceHandle>)startLoadingResource:(id <WebCoreResourceLoader>)loader withMethod:(NSString *)method URL:(NSURL *)URL customHeaders:(NSDictionary *)customHeaders postData:(NSArray *)data;
- (void)objectLoadedFromCacheWithURL:(NSURL *)URL response:(NSURLResponse *)response data:(NSData *)data;
- (NSData *)syncLoadResourceWithMethod:(NSString *)method URL:(NSURL *)URL customHeaders:(NSDictionary *)requestHeaders postData:(NSArray *)postData finalURL:(NSURL **)finalNSURL responseHeaders:(NSDictionary **)responseHeaderDict statusCode:(int *)statusCode;
- (BOOL)isReloading;
- (NSString *)incomingReferrer;
- (void)loadEmptyDocumentSynchronously;

@end

// The WebCoreFrameBridge protocol contains methods for use by the WebCore side of the bridge.

// In NSArray objects for post data, NSData objects represent literal data, and NSString objects represent encoded files.
// The encoding is the standard form encoding for uploading files.

@protocol WebCoreFrameBridge

- (NSView *)documentView;

- (WebCorePageBridge *)createWindowWithURL:(NSURL *)URL;
- (void)showWindow;

- (NSString *)userAgentForURL:(NSURL *)URL;

- (void)setStatusText:(NSString *)status;

- (WebCoreFrameBridge *)createChildFrameNamed:(NSString *)frameName withURL:(NSURL *)URL referrer:(const WebCore::String&)referrer ownerElement:(WebCoreElement *)ownerElement allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (BOOL)areToolbarsVisible;
- (void)setToolbarsVisible:(BOOL)visible;
- (BOOL)isStatusbarVisible;
- (void)setStatusbarVisible:(BOOL)visible;
- (BOOL)areScrollbarsVisible;
- (void)setScrollbarsVisible:(BOOL)visible;
- (NSWindow *)window;

- (void)setWindowIsResizable:(BOOL)resizable;
- (BOOL)windowIsResizable;

- (NSResponder *)firstResponder;
- (void)makeFirstResponder:(NSResponder *)responder;
- (void)willMakeFirstResponderForNodeFocus;

- (BOOL)textViewWasFirstResponderAtMouseDownTime:(NSTextView *)textView;

- (void)closeWindowSoon;

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message;
- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message;
- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result;
- (BOOL)shouldInterruptJavaScript;
- (BOOL)canRunBeforeUnloadConfirmPanel;
- (BOOL)runBeforeUnloadConfirmPanelWithMessage:(NSString *)message;
- (void)addMessageToConsole:(NSDictionary *)message;
- (void)runOpenPanelForFileButtonWithResultListener:(id <WebCoreOpenPanelResultListener>)resultListener;

- (time_t)expiresTimeForResponse:(NSURLResponse *)response;

- (void)activateWindow;
- (void)deactivateWindow;

- (void)formControlIsBecomingFirstResponder:(NSView *)formControl;
- (void)formControlIsResigningFirstResponder:(NSView *)formControl;

- (NSView *)nextKeyViewOutsideWebFrameViews;
- (NSView *)nextValidKeyViewOutsideWebFrameViews;
- (NSView *)previousKeyViewOutsideWebFrameViews;

- (BOOL)defersLoading;
- (void)setDefersLoading:(BOOL)loading;
- (void)saveDocumentState:(NSArray *)documentState;
- (NSArray *)documentState;

- (void)setNeedsReapplyStyles;

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                  attributeNames:(NSArray *)attributeNames
                 attributeValues:(NSArray *)attributeValues
                        MIMEType:(NSString *)MIMEType
                      DOMElement:(DOMElement *)element
                    loadManually:(BOOL)loadManually;
- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame
                        attributeNames:(NSArray *)attributeNames
                       attributeValues:(NSArray *)attributeValues
                               baseURL:(NSURL *)baseURL
                            DOMElement:(DOMElement *)element;
- (void)redirectDataToPlugin:(NSView *)pluginView;

- (BOOL)saveDocumentToPageCache:(id)documentInfo;

- (int)getObjectCacheSize;

- (ObjectElementType)determineObjectFromMIMEType:(NSString*)MIMEType URL:(NSURL*)URL;

- (NSString *)MIMETypeForPath:(NSString *)path;

- (void)allowDHTMLDrag:(BOOL *)flagDHTML UADrag:(BOOL *)flagUA;
- (BOOL)startDraggingImage:(NSImage *)dragImage at:(NSPoint)dragLoc operation:(NSDragOperation)op event:(NSEvent *)event sourceIsDHTML:(BOOL)flag DHTMLWroteData:(BOOL)dhtmlWroteData;
- (BOOL)mayStartDragAtEventLocation:(NSPoint)location;

- (BOOL)selectWordBeforeMenuEvent;

- (int)historyLength;
- (void)goBackOrForward:(int)distance;
- (BOOL)canGoBackOrForward:(int)distance;
- (NSURL *)historyURL:(int)distance;

- (void)textFieldDidBeginEditing:(DOMHTMLInputElement *)element;
- (void)textFieldDidEndEditing:(DOMHTMLInputElement *)element;
- (void)textDidChangeInTextField:(DOMHTMLInputElement *)element;
- (void)textDidChangeInTextArea:(DOMHTMLTextAreaElement *)element;

- (BOOL)textField:(DOMHTMLInputElement *)element doCommandBySelector:(SEL)commandSelector;
- (BOOL)textField:(DOMHTMLInputElement *)element shouldHandleEvent:(NSEvent *)event;

- (void)setHasBorder:(BOOL)hasBorder;

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
- (BOOL)shouldDeleteSelectedDOMRange:(DOMRange *)currentRange;
- (BOOL)shouldBeginEditing:(DOMRange *)range;
- (BOOL)shouldEndEditing:(DOMRange *)range;
- (void)didBeginEditing;
- (void)didEndEditing;
- (BOOL)canPaste;

- (NSString *)overrideMediaType;

- (void)windowObjectCleared;

- (int)spellCheckerDocumentTag;
- (BOOL)isContinuousSpellCheckingEnabled;

- (void)dashboardRegionsChanged:(NSMutableDictionary *)regions;
- (void)willPopupMenu:(NSMenu *)menu;

- (NSRect)customHighlightRect:(NSString*)type forLine:(NSRect)lineRect;
- (void)paintCustomHighlight:(NSString*)type forBox:(NSRect)boxRect onLine:(NSRect)lineRect behindText:(BOOL)text entireLine:(BOOL)line;

- (WebCoreKeyboardUIMode)keyboardUIMode;

- (NSString*)imageTitleForFilename:(NSString*)filename size:(NSSize)size;


@end

// This interface definition allows those who hold a WebCoreFrameBridge * to call all the methods
// in the WebCoreFrameBridge protocol without requiring the base implementation to supply the methods.
// This idiom is appropriate because WebCoreFrameBridge is an abstract class.

@interface WebCoreFrameBridge (SubclassResponsibility) <WebCoreFrameBridge>
@end

// Protocols that make up part of the interaces above.

@protocol WebCoreRenderTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name position:(NSPoint)p rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children;
@end

