/*	WebHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.
*/

#import <WebKit/WebHTMLView.h>

@class DOMDocumentFragment;
@class DOMRange;
@class WebArchive;
@class WebBridge;
@class WebView;
@class WebFrame;
@class WebPluginController;

@interface WebHTMLView (WebPrivate)

- (void)_reset;

// Modifier (flagsChanged) tracking SPI
+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent;
- (void)_updateMouseoverWithFakeEvent;

- (void)_setAsideSubviews;
- (void)_restoreSubviews;

- (BOOL)_insideAnotherHTMLView;
- (void)_clearLastHitViewIfSelf;
- (void)_updateMouseoverWithEvent:(NSEvent *)event;

+ (NSArray *)_insertablePasteboardTypes;
+ (NSArray *)_selectionPasteboardTypes;
- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard;
- (WebArchive *)_selectedArchive;

- (void)_frameOrBoundsChanged;

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element;
- (BOOL)_startDraggingImage:(NSImage *)dragImage at:(NSPoint)dragLoc operation:(NSDragOperation)op event:(NSEvent *)event sourceIsDHTML:(BOOL)flag DHTMLWroteData:(BOOL)dhtmlWroteData;
- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event;
- (BOOL)_mayStartDragAtEventLocation:(NSPoint)location;

- (WebPluginController *)_pluginController;

- (NSRect)_selectionRect;

- (void)_startAutoscrollTimer:(NSEvent *)event;
- (void)_stopAutoscrollTimer;

- (BOOL)_canCopy;
- (BOOL)_canCut;
- (BOOL)_canDelete;
- (BOOL)_canPaste;
- (BOOL)_canEdit;
- (BOOL)_hasSelection;
- (BOOL)_hasSelectionOrInsertionPoint;
- (BOOL)_isEditable;

- (BOOL)_isSelectionMisspelled;
- (NSArray *)_guessesForMisspelledSelection;

- (BOOL)_transparentBackground;
- (void)_setTransparentBackground:(BOOL)f;

// SPI's for Mail.
- (NSImage *)_selectionDraggingImage;
- (NSRect)_selectionDraggingRect;

@end
