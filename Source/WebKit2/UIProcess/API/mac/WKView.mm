/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WKView.h"

#import "ChunkedUpdateDrawingAreaProxy.h"
#import "DataReference.h"
#import "DrawingAreaProxyImpl.h"
#import "FindIndicator.h"
#import "FindIndicatorWindow.h"
#import "LayerBackedDrawingAreaProxy.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "PDFViewController.h"
#import "PageClientImpl.h"
#import "PasteboardTypes.h"
#import "PrintInfo.h"
#import "Region.h"
#import "RunLoop.h"
#import "TextChecker.h"
#import "TextCheckerState.h"
#import "WKAPICast.h"
#import "WKStringCF.h"
#import "WKTextInputWindowController.h"
#import "WebContext.h"
#import "WebEventFactory.h"
#import "WebPage.h"
#import "WebPageProxy.h"
#import "WebProcessManager.h"
#import "WebProcessProxy.h"
#import "WebSystemInterface.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DragController.h>
#import <WebCore/DragData.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/PlatformScreen.h>
#import <WebKitSystemInterface.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

// FIXME (WebKit2) <rdar://problem/8728860> WebKit2 needs to be localized
#define UI_STRING(__str, __desc) [NSString stringWithUTF8String:__str]

@interface NSApplication (Details)
- (void)speakString:(NSString *)string;
@end

@interface NSView (Details)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
@end

@interface NSWindow (Details)
- (NSRect)_growBoxRect;
- (BOOL)_updateGrowBoxForWindowFrameChange;
@end

extern "C" {
    // Need to declare this attribute name because AppKit exports it but does not make it available in API or SPI headers.
    // <rdar://problem/8631468> tracks the request to make it available. This code should be removed when the bug is closed.
    extern NSString *NSTextInputReplacementRangeAttributeName;
}

using namespace WebKit;
using namespace WebCore;

namespace WebKit {

typedef id <NSValidatedUserInterfaceItem> ValidationItem;
typedef Vector<RetainPtr<ValidationItem> > ValidationVector;
typedef HashMap<String, ValidationVector> ValidationMap;

}

NSString* const WebKitOriginalTopPrintingMarginKey = @"WebKitOriginalTopMargin";
NSString* const WebKitOriginalBottomPrintingMarginKey = @"WebKitOriginalBottomMargin";

@interface WKViewData : NSObject {
@public
    OwnPtr<PageClientImpl> _pageClient;
    RefPtr<WebPageProxy> _page;

    // For ToolTips.
    NSToolTipTag _lastToolTipTag;
    id _trackingRectOwner;
    void* _trackingRectUserData;

#if USE(ACCELERATED_COMPOSITING)
    NSView *_layerHostingView;
#endif

    RetainPtr<id> _remoteAccessibilityChild;
    
    // For asynchronous validation.
    ValidationMap _validationMap;

    OwnPtr<PDFViewController> _pdfViewController;

    OwnPtr<FindIndicatorWindow> _findIndicatorWindow;
    // We keep here the event when resending it to
    // the application to distinguish the case of a new event from one 
    // that has been already sent to WebCore.
    NSEvent *_keyDownEventBeingResent;
    Vector<KeypressCommand> _commandsList;

    // The identifier of the plug-in we want to send complex text input to, or 0 if there is none.
    uint64_t _pluginComplexTextInputIdentifier;

    Vector<CompositionUnderline> _underlines;
    unsigned _selectionStart;
    unsigned _selectionEnd;

    Vector<IntRect> _printingPageRects;
    double _totalScaleFactorForPrinting;

    bool _inBecomeFirstResponder;
    bool _inResignFirstResponder;
    NSEvent *_mouseDownEvent;
    BOOL _ignoringMouseDraggedEvents;
    BOOL _dragHasStarted;
}
@end

@implementation WKViewData
@end

@interface WebFrameWrapper : NSObject {
@public
    RefPtr<WebFrameProxy> _frame;
}

- (id)initWithFrameProxy:(WebFrameProxy*)frame;
- (WebFrameProxy*)webFrame;
@end

@implementation WebFrameWrapper

- (id)initWithFrameProxy:(WebFrameProxy*)frame
{
    self = [super init];
    if (!self)
        return nil;

    _frame = frame;
    return self;
}

- (WebFrameProxy*)webFrame
{
    return _frame.get();
}

@end

NSString * const PrintedFrameKey = @"WebKitPrintedFrameKey";

@interface NSObject (NSTextInputContextDetails)
- (BOOL)wantsToHandleMouseEvents;
- (BOOL)handleMouseEvent:(NSEvent *)event;
@end

@implementation WKView

static bool useNewDrawingArea()
{
    static bool useNewDrawingArea = getenv("USE_NEW_DRAWING_AREA");

    return useNewDrawingArea;
}

- (id)initWithFrame:(NSRect)frame
{
    return [self initWithFrame:frame contextRef:toAPI(WebContext::sharedProcessContext())];
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef
{   
    return [self initWithFrame:frame contextRef:contextRef pageGroupRef:nil];
}

- (void)_registerDraggedTypes
{
    NSMutableSet *types = [[NSMutableSet alloc] initWithArray:PasteboardTypes::forEditing()];
    [types addObjectsFromArray:PasteboardTypes::forURL()];
    [self registerForDraggedTypes:[types allObjects]];
    [types release];
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    InitWebCoreSystemInterface();
    RunLoop::initializeMainRunLoop();

    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:frame
                                                                options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
                                                                  owner:self
                                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];

    _data = [[WKViewData alloc] init];

    _data->_pageClient = PageClientImpl::create(self);
    _data->_page = toImpl(contextRef)->createWebPage(_data->_pageClient.get(), toImpl(pageGroupRef));
    _data->_page->initializeWebPage();
    _data->_mouseDownEvent = nil;
    _data->_ignoringMouseDraggedEvents = NO;

    [self _registerDraggedTypes];

    WebContext::statistics().wkViewCount++;

#if !defined(BUILDING_ON_SNOW_LEOPARD)
    NSData *remoteToken = (NSData *)WKAXRemoteTokenForElement(self);
    CoreIPC::DataReference dataToken = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    _data->_page->sendAccessibilityPresenterToken(dataToken);
#endif
    
    return self;
}

- (void)dealloc
{
    _data->_page->close();

    [_data release];

    WebContext::statistics().wkViewCount--;

    [super dealloc];
}

- (WKPageRef)pageRef
{
    return toAPI(_data->_page.get());
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    _data->_page->setDrawsBackground(drawsBackground);
}

- (BOOL)drawsBackground
{
    return _data->_page->drawsBackground();
}

- (void)setDrawsTransparentBackground:(BOOL)drawsTransparentBackground
{
    _data->_page->setDrawsTransparentBackground(drawsTransparentBackground);
}

- (BOOL)drawsTransparentBackground
{
    return _data->_page->drawsTransparentBackground();
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    NSSelectionDirection direction = [[self window] keyViewSelectionDirection];

    _data->_inBecomeFirstResponder = true;
    _data->_page->viewStateDidChange(WebPageProxy::ViewIsFocused);
    _data->_inBecomeFirstResponder = false;

    if (direction != NSDirectSelection)
        _data->_page->setInitialFocus(direction == NSSelectingNext);

    return YES;
}

- (BOOL)resignFirstResponder
{
    _data->_inResignFirstResponder = true;
    _data->_page->viewStateDidChange(WebPageProxy::ViewIsFocused);
    _data->_inResignFirstResponder = false;

    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];

    if (!_data->_page->drawingArea())
        return;
    
    _data->_page->drawingArea()->setSize(IntSize(size));
}

- (void)_updateWindowAndViewFrames
{
    NSWindow *window = [self window];
    ASSERT(window);
    
    NSRect windowFrameInScreenCoordinates = [window frame];
    NSRect viewFrameInWindowCoordinates = [self convertRect:[self frame] toView:nil];
    NSPoint accessibilityPosition = [[self accessibilityAttributeValue:NSAccessibilityPositionAttribute] pointValue];
    
    _data->_page->windowAndViewFramesChanged(enclosingIntRect(windowFrameInScreenCoordinates), enclosingIntRect(viewFrameInWindowCoordinates), IntPoint(accessibilityPosition));
}

- (void)renewGState
{
    // Hide the find indicator.
    _data->_findIndicatorWindow = nullptr;

    // Update the view frame.
    if ([self window])
        [self _updateWindowAndViewFrames];

    [super renewGState];
}

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap* createSelectorExceptionMap()
{
    SelectorNameMap* map = new HashMap<SEL, String>;
    
    map->add(@selector(insertNewlineIgnoringFieldEditor:), "InsertNewline");
    map->add(@selector(insertParagraphSeparator:), "InsertNewline");
    map->add(@selector(insertTabIgnoringFieldEditor:), "InsertTab");
    map->add(@selector(pageDown:), "MovePageDown");
    map->add(@selector(pageDownAndModifySelection:), "MovePageDownAndModifySelection");
    map->add(@selector(pageUp:), "MovePageUp");
    map->add(@selector(pageUpAndModifySelection:), "MovePageUpAndModifySelection");
    
    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap* exceptionMap = createSelectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap->find(selector);
    if (it != exceptionMap->end())
        return it->second;
    
    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are
    // not case sensitive.
    const char* selectorName = sel_getName(selector);
    size_t selectorNameLength = strlen(selectorName);
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return String(selectorName, selectorNameLength - 1);
}

// Editing commands

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { _data->_page->executeEditCommand(commandNameForSelector(_cmd)); }

WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(cut)
WEBCORE_COMMAND(paste)
WEBCORE_COMMAND(delete)
WEBCORE_COMMAND(pasteAsPlainText)
WEBCORE_COMMAND(selectAll)
WEBCORE_COMMAND(takeFindStringFromSelection)

#undef WEBCORE_COMMAND

// Menu items validation

static NSMenuItem *menuItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSMenuItem class]])
        return nil;
    return (NSMenuItem *)item;
}

static NSToolbarItem *toolbarItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSToolbarItem class]])
        return nil;
    return (NSToolbarItem *)item;
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(showGuessPanel:)) {
        if (NSMenuItem *menuItem = ::menuItem(item)) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING("Hide Spelling and Grammar", "menu item title")
                : UI_STRING("Show Spelling and Grammar", "menu item title")];
        }
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(checkSpelling:) || action == @selector(changeSpelling:))
        return _data->_page->selectionState().isContentEditable;

    if (action == @selector(toggleContinuousSpellChecking:)) {
        bool enabled = TextChecker::isContinuousSpellCheckingAllowed();
        bool checked = enabled && TextChecker::state().isContinuousSpellCheckingEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return enabled;
    }

    if (action == @selector(toggleGrammarChecking:)) {
        bool checked = TextChecker::state().isGrammarCheckingEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return YES;
    }

    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        bool checked = TextChecker::state().isAutomaticSpellingCorrectionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        if (NSMenuItem *menuItem = ::menuItem(item)) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible];
            [menuItem setTitle:panelShowing
                ? UI_STRING("Hide Substitutions", "menu item title")
                : UI_STRING("Show Substitutions", "menu item title")];
        }
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(toggleSmartInsertDelete:)) {
        bool checked = _data->_page->isSmartInsertDeleteEnabled();
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticDashSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticLinkDetection:)) {
        bool checked = TextChecker::state().isAutomaticLinkDetectionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticTextReplacement:)) {
        bool checked = TextChecker::state().isAutomaticTextReplacementEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return _data->_page->selectionState().isContentEditable;
    }

    if (action == @selector(uppercaseWord:) || action == @selector(lowercaseWord:) || action == @selector(capitalizeWord:))
        return _data->_page->selectionState().selectedRangeLength && _data->_page->selectionState().isContentEditable;
    
    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

    // Next, handle editor commands. Start by returning YES for anything that is not an editor command.
    // Returning YES is the default thing to do in an AppKit validate method for any selector that is not recognized.
    String commandName = commandNameForSelector([item action]);
    if (!Editor::commandIsSupportedFromMenuOrKeyBinding(commandName))
        return YES;

    // Add this item to the vector of items for a given command that are awaiting validation.
    pair<ValidationMap::iterator, bool> addResult = _data->_validationMap.add(commandName, ValidationVector());
    addResult.first->second.append(item);
    if (addResult.second) {
        // If we are not already awaiting validation for this command, start the asynchronous validation process.
        // FIXME: Theoretically, there is a race here; when we get the answer it might be old, from a previous time
        // we asked for the same command; there is no guarantee the answer is still valid.
        // FIXME: The function called here should be renamed validateCommand because it is not specific to menu items.
        _data->_page->validateMenuItem(commandName);
    }

    // Treat as enabled until we get the result back from the web process and _setUserInterfaceItemState is called.
    // FIXME <rdar://problem/8803459>: This means disabled items will flash enabled at first for a moment.
    // But returning NO here would be worse; that would make keyboard commands such as command-C fail.
    return YES;
}

static void speakString(WKStringRef string, WKErrorRef error, void*)
{
    if (error)
        return;
    if (!string)
        return;

    NSString *convertedString = toImpl(string)->string();
    [NSApp speakString:convertedString];
}

- (IBAction)startSpeaking:(id)sender
{
    _data->_page->getSelectionOrContentsAsString(StringCallback::create(0, speakString));
}

- (IBAction)stopSpeaking:(id)sender
{
    [NSApp stopSpeaking:sender];
}

- (IBAction)showGuessPanel:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *spellingPanel = [checker spellingPanel];
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:sender];
        return;
    }
    
    _data->_page->advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

- (IBAction)checkSpelling:(id)sender
{
    _data->_page->advanceToNextMisspelling(false);
}

- (void)changeSpelling:(id)sender
{
    NSString *word = [[sender selectedCell] stringValue];

    _data->_page->changeSpellingToWord(word);
}

- (IBAction)toggleContinuousSpellChecking:(id)sender
{
    bool spellCheckingEnabled = !TextChecker::state().isContinuousSpellCheckingEnabled;
    TextChecker::setContinuousSpellCheckingEnabled(spellCheckingEnabled);

    _data->_page->process()->updateTextCheckerState();

    if (!spellCheckingEnabled)
        _data->_page->unmarkAllMisspellings();
}

- (IBAction)toggleGrammarChecking:(id)sender
{
    bool grammarCheckingEnabled = !TextChecker::state().isGrammarCheckingEnabled;
    TextChecker::setGrammarCheckingEnabled(grammarCheckingEnabled);

    _data->_page->process()->updateTextCheckerState();

    if (!grammarCheckingEnabled)
        _data->_page->unmarkAllBadGrammar();
}

- (IBAction)toggleAutomaticSpellingCorrection:(id)sender
{
    TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);

    _data->_page->process()->updateTextCheckerState();
}

- (void)orderFrontSubstitutionsPanel:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *substitutionsPanel = [checker substitutionsPanel];
    if ([substitutionsPanel isVisible]) {
        [substitutionsPanel orderOut:sender];
        return;
    }
    [substitutionsPanel orderFront:sender];
}

- (IBAction)toggleSmartInsertDelete:(id)sender
{
    _data->_page->setSmartInsertDeleteEnabled(!_data->_page->isSmartInsertDeleteEnabled());
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticQuoteSubstitutionEnabled)
        return;

    TextChecker::setAutomaticQuoteSubstitutionEnabled(flag);
    _data->_page->process()->updateTextCheckerState();
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender
{
    TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
    _data->_page->process()->updateTextCheckerState();
}

- (BOOL)isAutomaticDashSubstitutionEnabled
{
    return TextChecker::state().isAutomaticDashSubstitutionEnabled;
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticDashSubstitutionEnabled)
        return;

    TextChecker::setAutomaticDashSubstitutionEnabled(flag);
    _data->_page->process()->updateTextCheckerState();
}

- (void)toggleAutomaticDashSubstitution:(id)sender
{
    TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
    _data->_page->process()->updateTextCheckerState();
}

- (BOOL)isAutomaticLinkDetectionEnabled
{
    return TextChecker::state().isAutomaticLinkDetectionEnabled;
}

- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticLinkDetectionEnabled)
        return;

    TextChecker::setAutomaticLinkDetectionEnabled(flag);
    _data->_page->process()->updateTextCheckerState();
}

- (void)toggleAutomaticLinkDetection:(id)sender
{
    TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
    _data->_page->process()->updateTextCheckerState();
}

- (BOOL)isAutomaticTextReplacementEnabled
{
    return TextChecker::state().isAutomaticTextReplacementEnabled;
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)flag
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticTextReplacementEnabled)
        return;

    TextChecker::setAutomaticTextReplacementEnabled(flag);
    _data->_page->process()->updateTextCheckerState();
}

- (void)toggleAutomaticTextReplacement:(id)sender
{
    TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
    _data->_page->process()->updateTextCheckerState();
}

- (void)uppercaseWord:(id)sender
{
    _data->_page->uppercaseWord();
}

- (void)lowercaseWord:(id)sender
{
    _data->_page->lowercaseWord();
}

- (void)capitalizeWord:(id)sender
{
    _data->_page->capitalizeWord();
}

// Events

// Override this so that AppKit will send us arrow keys as key down events so we can
// support them via the key bindings mechanism.
- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    return YES;
}

- (void)_setMouseDownEvent:(NSEvent *)event
{
    ASSERT(!event || [event type] == NSLeftMouseDown || [event type] == NSRightMouseDown || [event type] == NSOtherMouseDown);
    
    if (event == _data->_mouseDownEvent)
        return;
    
    [_data->_mouseDownEvent release];
    _data->_mouseDownEvent = [event retain];
}

#define EVENT_HANDLER(Selector, Type) \
    - (void)Selector:(NSEvent *)theEvent \
    { \
        Web##Type##Event webEvent = WebEventFactory::createWeb##Type##Event(theEvent, self); \
        _data->_page->handle##Type##Event(webEvent); \
    }

EVENT_HANDLER(mouseEntered, Mouse)
EVENT_HANDLER(mouseExited, Mouse)
EVENT_HANDLER(mouseMoved, Mouse)
EVENT_HANDLER(otherMouseDown, Mouse)
EVENT_HANDLER(otherMouseDragged, Mouse)
EVENT_HANDLER(otherMouseMoved, Mouse)
EVENT_HANDLER(otherMouseUp, Mouse)
EVENT_HANDLER(rightMouseDown, Mouse)
EVENT_HANDLER(rightMouseDragged, Mouse)
EVENT_HANDLER(rightMouseMoved, Mouse)
EVENT_HANDLER(rightMouseUp, Mouse)
EVENT_HANDLER(scrollWheel, Wheel)

#undef EVENT_HANDLER

- (void)_mouseHandler:(NSEvent *)event
{
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;
    WebMouseEvent webEvent = WebEventFactory::createWebMouseEvent(event, self);
    _data->_page->handleMouseEvent(webEvent);
}

- (void)mouseDown:(NSEvent *)event
{
    [self _setMouseDownEvent:event];
    _data->_ignoringMouseDraggedEvents = NO;
    _data->_dragHasStarted = NO;
    [self _mouseHandler:event];
}

- (void)mouseUp:(NSEvent *)event
{
    [self _setMouseDownEvent:nil];
    [self _mouseHandler:event];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (_data->_ignoringMouseDraggedEvents)
        return;
    [self _mouseHandler:event];
}

- (void)doCommandBySelector:(SEL)selector
{
    if (selector != @selector(noop:))
        _data->_commandsList.append(KeypressCommand(commandNameForSelector(selector)));
}

- (void)insertText:(id)string
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString
    
    LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);
    NSString *text;
    bool isFromInputMethod = _data->_page->selectionState().hasComposition;

    if (isAttributedString) {
        text = [string string];
        // We deal with the NSTextInputReplacementRangeAttributeName attribute from NSAttributedString here
        // simply because it is used by at least one Input Method -- it corresonds to the kEventParamTextInputSendReplaceRange
        // event in TSM.  This behaviour matches that of -[WebHTMLView setMarkedText:selectedRange:] when it receives an
        // NSAttributedString
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, [text length])];
        LOG(TextInput, "ReplacementRange: %@", rangeString);
        if (rangeString)
            isFromInputMethod = YES;
    } else
        text = string;
    
    String eventText = text;
    
    if (!isFromInputMethod)
        _data->_commandsList.append(KeypressCommand("insertText", text));
    else {
        eventText.replace(NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
        _data->_commandsList.append(KeypressCommand("insertText", eventText));
    }
}

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    if (([event modifierFlags] & NSDeviceIndependentModifierFlagsMask) != NSCommandKeyMask)
        return NO;
    
    // Here we special case cmd+b and cmd+i but not cmd+u, for historic reason.
    // This should not be changed, since it could break some Mac applications that
    // rely on this inherent behavior.
    // See https://bugs.webkit.org/show_bug.cgi?id=24943
    
    NSString *string = [event characters];
    if ([string caseInsensitiveCompare:@"b"] == NSOrderedSame) {
        _data->_page->executeEditCommand("ToggleBold");
        return YES;
    }
    if ([string caseInsensitiveCompare:@"i"] == NSOrderedSame) {
        _data->_page->executeEditCommand("ToggleItalic");
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
    
    BOOL eventWasSentToWebCore = (_data->_keyDownEventBeingResent == event);

    // Pass key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting key-modified keypresses.
    // But don't do it if we have already handled the event.
    // Pressing Esc results in a fake event being sent - don't pass it to WebCore.
    if (!eventWasSentToWebCore && event == [NSApp currentEvent] && self == [[self window] firstResponder]) {
        [_data->_keyDownEventBeingResent release];
        _data->_keyDownEventBeingResent = nil;
        
        _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(event, self));
        return YES;
    }
    
    return [self _handleStyleKeyEquivalent:event] || [super performKeyEquivalent:event];
}

- (void)_setEventBeingResent:(NSEvent *)event
{
    _data->_keyDownEventBeingResent = [event retain];
}

- (Vector<KeypressCommand>&)_interceptKeyEvent:(NSEvent *)theEvent 
{
    _data->_commandsList.clear();
    // interpretKeyEvents will trigger one or more calls to doCommandBySelector or setText
    // that will populate the commandsList vector.
    [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
    return _data->_commandsList;
}

- (void)_getTextInputState:(unsigned)start selectionEnd:(unsigned)end underlines:(Vector<WebCore::CompositionUnderline>&)lines
{
    start = _data->_selectionStart;
    end = _data->_selectionEnd;
    lines = _data->_underlines;
}

- (void)keyUp:(NSEvent *)theEvent
{
    _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent, self));
}

- (void)keyDown:(NSEvent *)theEvent
{
    if (_data->_pluginComplexTextInputIdentifier) {
        // Try feeding the keyboard event directly to the plug-in.
        NSString *string = nil;
        if ([[WKTextInputWindowController sharedTextInputWindowController] interpretKeyEvent:theEvent string:&string]) {
            if (string)
                _data->_page->sendComplexTextInputToPlugin(_data->_pluginComplexTextInputIdentifier, string);
            return;
        }
    }

    _data->_underlines.clear();
    _data->_selectionStart = 0;
    _data->_selectionEnd = 0;
    // We could be receiving a key down from AppKit if we have re-sent an event
    // that maps to an action that is currently unavailable (for example a copy when
    // there is no range selection).
    // If this is the case we should ignore the key down.
    if (_data->_keyDownEventBeingResent == theEvent) {
        [_data->_keyDownEventBeingResent release];
        _data->_keyDownEventBeingResent = nil;
        [super keyDown:theEvent];
        return;
    }
    _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent, self));
}

- (NSTextInputContext *)inputContext {
    if (_data->_pluginComplexTextInputIdentifier)
        return [[WKTextInputWindowController sharedTextInputWindowController] inputContext];

    return [super inputContext];
}

- (NSRange)selectedRange
{
    if (_data->_page->selectionState().isNone || !_data->_page->selectionState().isContentEditable)
        return NSMakeRange(NSNotFound, 0);
    
    LOG(TextInput, "selectedRange -> (%u, %u)", _data->_page->selectionState().selectedRangeStart, _data->_page->selectionState().selectedRangeLength);
    return NSMakeRange(_data->_page->selectionState().selectedRangeStart, _data->_page->selectionState().selectedRangeLength);
}

- (BOOL)hasMarkedText
{
    LOG(TextInput, "hasMarkedText -> %u", _data->_page->selectionState().hasComposition);
    return _data->_page->selectionState().hasComposition;
}

- (void)unmarkText
{
    LOG(TextInput, "unmarkText");
    
    _data->_commandsList.append(KeypressCommand("unmarkText"));
}

- (NSArray *)validAttributesForMarkedText
{
    static NSArray *validAttributes;
    if (!validAttributes) {
        validAttributes = [[NSArray alloc] initWithObjects:
                           NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
                           NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
        // NSText also supports the following attributes, but it's
        // hard to tell which are really required for text input to
        // work well; I have not seen any input method make use of them yet.
        //     NSFontAttributeName, NSForegroundColorAttributeName,
        //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
        CFRetain(validAttributes);
    }
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes;
}

static void extractUnderlines(NSAttributedString *string, Vector<CompositionUnderline>& result)
{
    int length = [[string string] length];
    
    int i = 0;
    while (i < length) {
        NSRange range;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];
        
        if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            Color color = Color::black;
            if (NSColor *colorAttr = [attrs objectForKey:NSUnderlineColorAttributeName])
                color = colorFromNSColor([colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            result.append(CompositionUnderline(range.location, NSMaxRange(range), color, [style intValue] > 1));
        }
        
        i = range.location + range.length;
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString
    
    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%u, %u)", isAttributedString ? [string string] : string, newSelRange.location, newSelRange.length);
    
    NSString *text = string;
    
    if (isAttributedString) {
        text = [string string];
        extractUnderlines(string, _data->_underlines);
    }
    
    _data->_commandsList.append(KeypressCommand("setMarkedText", text));
    _data->_selectionStart = newSelRange.location;
    _data->_selectionEnd = NSMaxRange(newSelRange);
}

- (NSRange)markedRange
{
    uint64_t location;
    uint64_t length;

    _data->_page->getMarkedRange(location, length);
    LOG(TextInput, "markedRange -> (%u, %u)", location, length);
    return NSMakeRange(location, length);
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange
{
    // This is not implemented for now. Need to figure out how to serialize the attributed string across processes.
    LOG(TextInput, "attributedSubstringFromRange");
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    NSWindow *window = [self window];
    
    if (window)
        thePoint = [window convertScreenToBase:thePoint];
    thePoint = [self convertPoint:thePoint fromView:nil];  // the point is relative to the main frame 
    
    uint64_t result = _data->_page->characterIndexForPoint(IntPoint(thePoint));
    LOG(TextInput, "characterIndexForPoint:(%f, %f) -> %u", thePoint.x, thePoint.y, result);
    return result;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{ 
    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((theRange.location + theRange.length < theRange.location) && (theRange.location + theRange.length != 0))
        theRange.length = 0;
    
    NSRect resultRect = _data->_page->firstRectForCharacterRange(theRange.location, theRange.length);
    resultRect = [self convertRect:resultRect toView:nil];
    
    NSWindow *window = [self window];
    if (window)
        resultRect.origin = [window convertBaseToScreen:resultRect.origin];
    
    LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (%f, %f, %f, %f)", theRange.location, theRange.length, resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
    return resultRect;
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = windowImageLoc;
   
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _data->_ignoringMouseDraggedEvents = YES;
    
    _data->_page->dragEnded(IntPoint(windowMouseLoc), globalPoint(windowMouseLoc, [self window]), operation);
}

- (DragApplicationFlags)applicationFlags:(id <NSDraggingInfo>)draggingInfo
{
    uint32_t flags = 0;
    if ([NSApp modalWindow])
        flags = DragApplicationIsModal;
    if ([[self window] attachedSheet])
        flags |= DragApplicationHasAttachedSheet;
    if ([draggingInfo draggingSource] == self)
        flags |= DragApplicationIsSource;
    if ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask)
        flags |= DragApplicationIsCopyKeyDown;
    return static_cast<DragApplicationFlags>(flags);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)draggingInfo
{
    IntPoint client([self convertPoint:[draggingInfo draggingLocation] fromView:nil]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, static_cast<DragOperation>([draggingInfo draggingSourceOperationMask]), [self applicationFlags:draggingInfo]);

    _data->_page->resetDragOperation();
    _data->_page->performDragControllerAction(DragControllerActionEntered, &dragData, [[draggingInfo draggingPasteboard] name]);
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)draggingInfo
{
    IntPoint client([self convertPoint:[draggingInfo draggingLocation] fromView:nil]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, static_cast<DragOperation>([draggingInfo draggingSourceOperationMask]), [self applicationFlags:draggingInfo]);
    _data->_page->performDragControllerAction(DragControllerActionUpdated, &dragData, [[draggingInfo draggingPasteboard] name]);
    return _data->_page->dragOperation();
}

- (void)draggingExited:(id <NSDraggingInfo>)draggingInfo
{
    IntPoint client([self convertPoint:[draggingInfo draggingLocation] fromView:nil]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, static_cast<DragOperation>([draggingInfo draggingSourceOperationMask]), [self applicationFlags:draggingInfo]);
    _data->_page->performDragControllerAction(DragControllerActionExited, &dragData, [[draggingInfo draggingPasteboard] name]);
    _data->_page->resetDragOperation();
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)draggingInfo
{
    IntPoint client([self convertPoint:[draggingInfo draggingLocation] fromView:nil]);
    IntPoint global(globalPoint([draggingInfo draggingLocation], [self window]));
    DragData dragData(draggingInfo, client, global, static_cast<DragOperation>([draggingInfo draggingSourceOperationMask]), [self applicationFlags:draggingInfo]);
    _data->_page->performDragControllerAction(DragControllerActionPerformDrag, &dragData, [[draggingInfo draggingPasteboard] name]);
    return YES;
}

- (void)_updateWindowVisibility
{
    _data->_page->updateWindowIsVisible(![[self window] isMiniaturized]);
}

- (BOOL)_ownsWindowGrowBox
{
    NSWindow* window = [self window];
    if (!window)
        return NO;

    NSView *superview = [self superview];
    if (!superview)
        return NO;

    NSRect growBoxRect = [window _growBoxRect];
    if (NSIsEmptyRect(growBoxRect))
        return NO;

    NSRect visibleRect = [self visibleRect];
    if (NSIsEmptyRect(visibleRect))
        return NO;

    NSRect visibleRectInWindowCoords = [self convertRect:visibleRect toView:nil];
    if (!NSIntersectsRect(growBoxRect, visibleRectInWindowCoords))
        return NO;

    return YES;
}

- (BOOL)_updateGrowBoxForWindowFrameChange
{
    // Temporarily enable the resize indicator to make a the _ownsWindowGrowBox calculation work.
    BOOL wasShowingIndicator = [[self window] showsResizeIndicator];
    [[self window] setShowsResizeIndicator:YES];

    BOOL ownsGrowBox = [self _ownsWindowGrowBox];
    _data->_page->setWindowResizerSize(ownsGrowBox ? enclosingIntRect([[self window] _growBoxRect]).size() : IntSize());
    
    // Once WebCore can draw the window resizer, this should read:
    // if (wasShowingIndicator)
    //     [[self window] setShowsResizeIndicator:!ownsGrowBox];
    [[self window] setShowsResizeIndicator:wasShowingIndicator];

    return ownsGrowBox;
}

- (void)addWindowObserversForWindow:(NSWindow *)window
{
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidBecomeKey:)
                                                     name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResignKey:)
                                                     name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidMiniaturize:) 
                                                     name:NSWindowDidMiniaturizeNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidDeminiaturize:)
                                                     name:NSWindowDidDeminiaturizeNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowFrameDidChange:)
                                                     name:NSWindowDidMoveNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowFrameDidChange:) 
                                                     name:NSWindowDidResizeNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (!window)
        return;

    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidMiniaturizeNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidMoveNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResizeNotification object:window];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if (window != [self window]) {
        [self removeWindowObservers];
        [self addWindowObserversForWindow:window];
    }
}

- (void)viewDidMoveToWindow
{
    // We want to make sure to update the active state while hidden, so if the view is about to become visible, we
    // update the active state first and then make it visible. If the view is about to be hidden, we hide it first and then
    // update the active state.
    if ([self window]) {
        _data->_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
        _data->_page->viewStateDidChange(WebPageProxy::ViewIsVisible | WebPageProxy::ViewIsInWindow);
        [self _updateWindowVisibility];
        [self _updateWindowAndViewFrames];
    } else {
        _data->_page->viewStateDidChange(WebPageProxy::ViewIsVisible);
        _data->_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive | WebPageProxy::ViewIsInWindow);
    }

}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *keyWindow = [notification object];
    if (keyWindow == [self window] || keyWindow == [[self window] attachedSheet])
        _data->_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    NSWindow *formerKeyWindow = [notification object];
    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet])
        _data->_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
}

- (void)_windowDidMiniaturize:(NSNotification *)notification
{
    [self _updateWindowVisibility];
}

- (void)_windowDidDeminiaturize:(NSNotification *)notification
{
    [self _updateWindowVisibility];
}

- (void)_windowFrameDidChange:(NSNotification *)notification
{
    [self _updateWindowAndViewFrames];
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "drawRect: x:%g, y:%g, width:%g, height:%g", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    if (useNewDrawingArea()) {
        if (DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(_data->_page->drawingArea())) {
            CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);

            const NSRect *rectsBeingDrawn;
            NSInteger numRectsBeingDrawn;
            [self getRectsBeingDrawn:&rectsBeingDrawn count:&numRectsBeingDrawn];
            for (NSInteger i = 0; i < numRectsBeingDrawn; ++i) {
                Region unpaintedRegion;
                IntRect rect = enclosingIntRect(rectsBeingDrawn[i]);
                drawingArea->paint(context, rect, unpaintedRegion);
            }
        } else if (_data->_page->drawsBackground()) {
            [_data->_page->drawsTransparentBackground() ? [NSColor clearColor] : [NSColor whiteColor] set];
            NSRectFill(rect);
        }

        _data->_page->didDraw();
        return;
    }

    if (_data->_page->isValid() && _data->_page->drawingArea()) {
        CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
        _data->_page->drawingArea()->paint(IntRect(rect), context);
        _data->_page->didDraw();
    } else if (_data->_page->drawsBackground()) {
        [_data->_page->drawsTransparentBackground() ? [NSColor clearColor] : [NSColor whiteColor] set];
        NSRectFill(rect);
    }
}

- (BOOL)isOpaque
{
    return _data->_page->drawsBackground();
}

- (void)viewDidHide
{
    _data->_page->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

- (void)viewDidUnhide
{
    _data->_page->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

- (void)_setAccessibilityChildToken:(NSData *)data
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    _data->_remoteAccessibilityChild = WKAXRemoteElementForToken((CFDataRef)data);
    WKAXInitializeRemoteElementWithWindow(_data->_remoteAccessibilityChild.get(), [self window]);
#endif
}

- (BOOL)accessibilityIsIgnored
{
    return NO;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return _data->_remoteAccessibilityChild.get();
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        if (!_data->_remoteAccessibilityChild)
            return nil;
        return [NSArray arrayWithObject:_data->_remoteAccessibilityChild.get()];
    }
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return NSAccessibilityUnignoredAncestor([self superview]);
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool:YES];
    
    return [super accessibilityAttributeValue:attribute];
}

- (NSView *)hitTest:(NSPoint)point
{
    NSView *hitView = [super hitTest:point];
#if USE(ACCELERATED_COMPOSITING)
    if (hitView && _data && hitView == _data->_layerHostingView)
        hitView = self;
#endif
    return hitView;
}

- (NSInteger)conversationIdentifier
{
    return (NSInteger)self;
}

static void setFrameBeingPrinted(NSPrintOperation *printOperation, WebFrameProxy* frame)
{
    RetainPtr<WebFrameWrapper> frameWrapper(AdoptNS, [[WebFrameWrapper alloc] initWithFrameProxy:frame]);
    [[[printOperation printInfo] dictionary] setObject:frameWrapper.get() forKey:PrintedFrameKey];
}

static WebFrameProxy* frameBeingPrinted()
{
    return [[[[[NSPrintOperation currentOperation] printInfo] dictionary] objectForKey:PrintedFrameKey] webFrame];
}

static float currentPrintOperationScale()
{
    ASSERT([NSPrintOperation currentOperation]);
    ASSERT([[[[NSPrintOperation currentOperation] printInfo] dictionary] objectForKey:NSPrintScalingFactor]);
    return [[[[[NSPrintOperation currentOperation] printInfo] dictionary] objectForKey:NSPrintScalingFactor] floatValue];
}

- (void)_adjustPrintingMarginsForHeaderAndFooter
{
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    NSPrintInfo *info = [printOperation printInfo];
    NSMutableDictionary *infoDictionary = [info dictionary];

    // We need to modify the top and bottom margins in the NSPrintInfo to account for the space needed by the
    // header and footer. Because this method can be called more than once on the same NSPrintInfo (see 5038087),
    // we stash away the unmodified top and bottom margins the first time this method is called, and we read from
    // those stashed-away values on subsequent calls.
    float originalTopMargin;
    float originalBottomMargin;
    NSNumber *originalTopMarginNumber = [infoDictionary objectForKey:WebKitOriginalTopPrintingMarginKey];
    if (!originalTopMarginNumber) {
        ASSERT(![infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey]);
        originalTopMargin = [info topMargin];
        originalBottomMargin = [info bottomMargin];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalTopMargin] forKey:WebKitOriginalTopPrintingMarginKey];
        [infoDictionary setObject:[NSNumber numberWithFloat:originalBottomMargin] forKey:WebKitOriginalBottomPrintingMarginKey];
    } else {
        ASSERT([originalTopMarginNumber isKindOfClass:[NSNumber class]]);
        ASSERT([[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] isKindOfClass:[NSNumber class]]);
        originalTopMargin = [originalTopMarginNumber floatValue];
        originalBottomMargin = [[infoDictionary objectForKey:WebKitOriginalBottomPrintingMarginKey] floatValue];
    }
    
    float scale = currentPrintOperationScale();
    [info setTopMargin:originalTopMargin + _data->_page->headerHeight(frameBeingPrinted()) * scale];
    [info setBottomMargin:originalBottomMargin + _data->_page->footerHeight(frameBeingPrinted()) * scale];
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo forFrame:(WKFrameRef)frameRef
{
    LOG(View, "Creating an NSPrintOperation for frame '%s'", toImpl(frameRef)->url().utf8().data());
    NSPrintOperation *printOperation;

    // Only the top frame can currently contain a PDF view.
    if (_data->_pdfViewController) {
        ASSERT(toImpl(frameRef)->isMainFrame());
        printOperation = _data->_pdfViewController->makePrintOperation(printInfo);
    } else
        printOperation = [NSPrintOperation printOperationWithView:self printInfo:printInfo];

    setFrameBeingPrinted(printOperation, toImpl(frameRef));
    return printOperation;
}

- (BOOL)canChangeFrameLayout:(WKFrameRef)frameRef
{
    // PDF documents are already paginated, so we can't change them to add headers and footers.
    return !toImpl(frameRef)->isMainFrame() || !_data->_pdfViewController;
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range
{
    LOG(View, "knowsPageRange:");
    WebFrameProxy* frame = frameBeingPrinted();
    ASSERT(frame);

    if (frame->isMainFrame() && _data->_pdfViewController)
        return [super knowsPageRange:range];

    [self _adjustPrintingMarginsForHeaderAndFooter];

    _data->_page->computePagesForPrinting(frame, PrintInfo([[NSPrintOperation currentOperation] printInfo]), _data->_printingPageRects, _data->_totalScaleFactorForPrinting);

    *range = NSMakeRange(1, _data->_printingPageRects.size());
    return YES;
}

// Take over printing. AppKit applies incorrect clipping, and doesn't print pages beyond the first one.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // FIXME: This check isn't right for some non-printing cases, such as capturing into a buffer using cacheDisplayInRect:toBitmapImageRep:.
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        _data->_page->endPrinting();
        [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect rectIsVisibleRectForView:visibleView topView:topView];
        return;
    }

    LOG(View, "Printing rect x:%g, y:%g, width:%g, height:%g", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

    ASSERT(self == visibleView);
    ASSERT(frameBeingPrinted());

    WebFrameProxy* frame = frameBeingPrinted();
    ASSERT(frame);

    _data->_page->beginPrinting(frame, PrintInfo([[NSPrintOperation currentOperation] printInfo]));

    // FIXME: This is optimized for print preview. Get the whole document at once when actually printing.
    Vector<uint8_t> pdfData;
    _data->_page->drawRectToPDF(frame, IntRect(rect), pdfData);

    RetainPtr<CGDataProviderRef> pdfDataProvider(AdoptCF, CGDataProviderCreateWithData(0, pdfData.data(), pdfData.size(), 0));
    RetainPtr<CGPDFDocumentRef> pdfDocument(AdoptCF, CGPDFDocumentCreateWithProvider(pdfDataProvider.get()));
    if (!pdfDocument) {
        LOG_ERROR("Couldn't create a PDF document with data passed for printing");
        return;
    }

    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdfDocument.get(), 1);
    if (!pdfPage) {
        LOG_ERROR("Printing data doesn't have page 1");
        return;
    }

    NSGraphicsContext *nsGraphicsContext = [NSGraphicsContext currentContext];
    CGContextRef context = static_cast<CGContextRef>([nsGraphicsContext graphicsPort]);

    CGContextSaveGState(context);
    // Flip the destination.
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -rect.size.height);
    CGContextDrawPDFPage(context, pdfPage);
    CGContextRestoreGState(context);
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    

    // The header and footer rect height scales with the page, but the width is always
    // all the way across the printed page (inset by printing margins).
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    NSPrintInfo *printInfo = [printOperation printInfo];
    float scale = currentPrintOperationScale();
    NSSize paperSize = [printInfo paperSize];
    float headerFooterLeft = [printInfo leftMargin] / scale;
    float headerFooterWidth = (paperSize.width - ([printInfo leftMargin] + [printInfo rightMargin])) / scale;
    WebFrameProxy* frame = frameBeingPrinted();
    NSRect footerRect = NSMakeRect(headerFooterLeft, [printInfo bottomMargin] / scale - _data->_page->footerHeight(frame), headerFooterWidth, _data->_page->footerHeight(frame));
    NSRect headerRect = NSMakeRect(headerFooterLeft, (paperSize.height - [printInfo topMargin]) / scale, headerFooterWidth, _data->_page->headerHeight(frame));

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    NSRectClip(headerRect);
    _data->_page->drawHeader(frame, headerRect);
    [currentContext restoreGraphicsState];

    [currentContext saveGraphicsState];
    NSRectClip(footerRect);
    _data->_page->drawFooter(frame, footerRect);
    [currentContext restoreGraphicsState];
}

// FIXME 3491344: This is an AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (CGFloat)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return _data->_totalScaleFactorForPrinting;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(NSInteger)page
{
    WebFrameProxy* frame = frameBeingPrinted();
    ASSERT(frame);

    if (frame->isMainFrame() && _data->_pdfViewController)
        return [super rectForPage:page];

    LOG(View, "rectForPage:%d -> x %d, y %d, width %d, height %d\n", (int)page, _data->_printingPageRects[page - 1].x(), _data->_printingPageRects[page - 1].y(), _data->_printingPageRects[page - 1].width(), _data->_printingPageRects[page - 1].height());
    return _data->_printingPageRects[page - 1];
}

@end

@implementation WKView (Internal)

- (PassOwnPtr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy
{
    if (useNewDrawingArea())
        return DrawingAreaProxyImpl::create(_data->_page.get());

    return ChunkedUpdateDrawingAreaProxy::create(self, _data->_page.get());
}

- (BOOL)_isFocused
{
    if (_data->_inBecomeFirstResponder)
        return YES;
    if (_data->_inResignFirstResponder)
        return NO;
    return [[self window] firstResponder] == self;
}

- (void)_processDidCrash
{
    [self setNeedsDisplay:YES];
}

- (void)_didRelaunchProcess
{
    [self setNeedsDisplay:YES];
}

- (void)_takeFocus:(BOOL)forward
{
    if (forward)
        [[self window] selectKeyViewFollowingView:self];
    else
        [[self window] selectKeyViewPrecedingView:self];
}

- (void)_setCursor:(NSCursor *)cursor
{
    if ([NSCursor currentCursor] == cursor)
        return;
    [cursor set];
}

- (void)_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState
{
    ValidationVector items = _data->_validationMap.take(commandName);
    size_t size = items.size();
    for (size_t i = 0; i < size; ++i) {
        ValidationItem item = items[i].get();
        [menuItem(item) setState:newState];
        [menuItem(item) setEnabled:isEnabled];
        [toolbarItem(item) setEnabled:isEnabled];
        // FIXME <rdar://problem/8803392>: If the item is neither a menu nor toolbar item, it will be left enabled.
    }
}

- (NSRect)_convertToDeviceSpace:(NSRect)rect
{
    return toDeviceSpace(rect, [self window]);
}

- (NSRect)_convertToUserSpace:(NSRect)rect
{
    return toUserSpace(rect, [self window]);
}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_data && (tag == TRACKING_RECT_TAG)) {
        _data->_trackingRectOwner = nil;
        return;
    }
    
    if (_data && (tag == _data->_lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _data->_lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_data != nil) {
            _data->_trackingRectOwner = nil;
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
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseExited:fakeEvent];
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
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseEntered:fakeEvent];
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return nsStringFromWebCoreString(_data->_page->toolTip());
}

- (void)_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip
{
    if (oldToolTip)
        [self _sendToolTipMouseExited];

    if (newToolTip && [newToolTip length] > 0) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _data->_lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (void)_setFindIndicator:(PassRefPtr<FindIndicator>)findIndicator fadeOut:(BOOL)fadeOut
{
    if (!findIndicator) {
        _data->_findIndicatorWindow = 0;
        return;
    }

    if (!_data->_findIndicatorWindow)
        _data->_findIndicatorWindow = FindIndicatorWindow::create(self);

    _data->_findIndicatorWindow->setFindIndicator(findIndicator, fadeOut);
}

#if USE(ACCELERATED_COMPOSITING)
- (void)_startAcceleratedCompositing:(CALayer *)rootLayer
{
    if (!_data->_layerHostingView) {
        NSView *hostingView = [[NSView alloc] initWithFrame:[self bounds]];
#if !defined(BUILDING_ON_LEOPARD)
        [hostingView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
#endif
        
        [self addSubview:hostingView];
        [hostingView release];
        _data->_layerHostingView = hostingView;
    }

    // Make a container layer, which will get sized/positioned by AppKit and CA.
    CALayer *viewLayer = [CALayer layer];

#ifndef NDEBUG
    [viewLayer setName:@"hosting layer"];
#endif

#if defined(BUILDING_ON_LEOPARD)
    // Turn off default animations.
    NSNull *nullValue = [NSNull null];
    NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nil];
    [viewLayer setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];
#endif

#if !defined(BUILDING_ON_LEOPARD)
    // If we aren't in the window yet, we'll use the screen's scale factor now, and reset the scale 
    // via -viewDidMoveToWindow.
    CGFloat scaleFactor = [self window] ? [[self window] userSpaceScaleFactor] : [[NSScreen mainScreen] userSpaceScaleFactor];
    [viewLayer setTransform:CATransform3DMakeScale(scaleFactor, scaleFactor, 1)];
#endif

    [_data->_layerHostingView setLayer:viewLayer];
    [_data->_layerHostingView setWantsLayer:YES];
    
    // Parent our root layer in the container layer
    [viewLayer addSublayer:rootLayer];
}

- (void)_stopAcceleratedCompositing
{
    if (_data->_layerHostingView) {
        [_data->_layerHostingView setLayer:nil];
        [_data->_layerHostingView setWantsLayer:NO];
        [_data->_layerHostingView removeFromSuperview];
        _data->_layerHostingView = nil;
    }
}

- (void)_switchToDrawingAreaTypeIfNecessary:(DrawingAreaInfo::Type)type
{
    DrawingAreaInfo::Type existingDrawingAreaType = _data->_page->drawingArea() ? _data->_page->drawingArea()->info().type : DrawingAreaInfo::None;
    if (existingDrawingAreaType == type)
        return;

    OwnPtr<DrawingAreaProxy> newDrawingArea;
    switch (type) {
        case DrawingAreaInfo::Impl:
        case DrawingAreaInfo::None:
            break;
        case DrawingAreaInfo::ChunkedUpdate: {
            newDrawingArea = ChunkedUpdateDrawingAreaProxy::create(self, _data->_page.get());
            break;
        }
        case DrawingAreaInfo::LayerBacked: {
            newDrawingArea = LayerBackedDrawingAreaProxy::create(self, _data->_page.get());
            break;
        }
    }

    newDrawingArea->setSize(IntSize([self frame].size));

    _data->_page->drawingArea()->detachCompositingContext();
    _data->_page->setDrawingArea(newDrawingArea.release());
}

- (void)_pageDidEnterAcceleratedCompositing
{
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaInfo::LayerBacked];
}

- (void)_pageDidLeaveAcceleratedCompositing
{
    // FIXME: we may want to avoid flipping back to the non-layer-backed drawing area until the next page load, to avoid thrashing.
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaInfo::ChunkedUpdate];
}
#endif // USE(ACCELERATED_COMPOSITING)

- (void)_setComplexTextInputEnabled:(BOOL)complexTextInputEnabled pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier
{
    BOOL inputSourceChanged = _data->_pluginComplexTextInputIdentifier;

    if (complexTextInputEnabled) {
        // Check if we're already allowing text input for this plug-in.
        if (pluginComplexTextInputIdentifier == _data->_pluginComplexTextInputIdentifier)
            return;

        _data->_pluginComplexTextInputIdentifier = pluginComplexTextInputIdentifier;

    } else {
        // Check if we got a request to disable complex text input for a plug-in that is not the current plug-in.
        if (pluginComplexTextInputIdentifier != _data->_pluginComplexTextInputIdentifier)
            return;

        _data->_pluginComplexTextInputIdentifier = 0;
    }

    if (inputSourceChanged) {
        // Inform the out of line window that the input source changed.
        [[WKTextInputWindowController sharedTextInputWindowController] keyboardInputSourceChanged];
    }
}

- (void)_setPageHasCustomRepresentation:(BOOL)pageHasCustomRepresentation
{
    _data->_pdfViewController = nullptr;

    if (pageHasCustomRepresentation)
        _data->_pdfViewController = PDFViewController::create(self);
}

- (void)_didFinishLoadingDataForCustomRepresentation:(const CoreIPC::DataReference&)dataReference
{
    ASSERT(_data->_pdfViewController);

    _data->_pdfViewController->setPDFDocumentData(_data->_page->mainFrame()->mimeType(), dataReference);
}

- (double)_customRepresentationZoomFactor
{
    if (!_data->_pdfViewController)
        return 1;

    return _data->_pdfViewController->zoomFactor();
}

- (void)_setCustomRepresentationZoomFactor:(double)zoomFactor
{
    if (!_data->_pdfViewController)
        return;

    _data->_pdfViewController->setZoomFactor(zoomFactor);
}

- (void)_setDragImage:(NSImage *)image at:(NSPoint)clientPoint linkDrag:(BOOL)linkDrag
{
    // We need to prevent re-entering this call to avoid crashing in AppKit.
    // Given the asynchronous nature of WebKit2 this can now happen.
    if (_data->_dragHasStarted)
        return;
    
    _data->_dragHasStarted = YES;
    [super dragImage:image
                  at:clientPoint
              offset:NSZeroSize
               event:(linkDrag) ? [NSApp currentEvent] :_data->_mouseDownEvent
          pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
              source:self
           slideBack:YES];
    _data->_dragHasStarted = NO;
}

@end
