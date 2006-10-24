// -*- c-basic-offset: 2 -*-
 /* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef Frame_H
#define Frame_H

#include "Color.h"
#include "EditAction.h"
#include "FrameView.h"
#include "KURL.h"
#include "Node.h"
#include "RenderObject.h"
#include "RenderLayer.h"
#include "ScrollBar.h"
#include "TextAffinity.h"
#include "TextGranularity.h"
#include <wtf/Vector.h>

namespace KJS {
    class JSValue;
    class PausedTimeouts;
    class SavedBuiltins;
    class SavedProperties;

    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

namespace WebCore {

class BrowserExtension;
class CommandByName;
class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class CSSStyleDeclaration;
class DrawContentsEvent;
class DOMWindow;
class EditCommand;
class Editor;
class EditorClient;
class FormData;
class FramePrivate;
class FrameLoadRequest;
class FrameLoader;
class FrameTree;
class KJSProxy;
class Page;
class Plugin;
class MouseEventWithHitTestResults;
class Range;
class RenderLayer;
class ResourceRequest;
class Selection;
class SelectionController;
class Settings;
class VisiblePosition;

template <typename T> class Timer;

struct MarkedTextUnderline {
    MarkedTextUnderline() 
        : startOffset(0), endOffset(0), thick(false) { }
    MarkedTextUnderline(unsigned s, unsigned e, const Color& c, bool t) 
        : startOffset(s), endOffset(e), color(c), thick(t) { }
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
};

enum ObjectContentType {
    ObjectContentNone,
    ObjectContentImage,
    ObjectContentFrame,
    ObjectContentPlugin
};

class Frame : public Shared<Frame> {
public:
  enum { NoXPosForVerticalArrowNavigation = INT_MIN };

  Frame(Page*, Element*, PassRefPtr<EditorClient>);
  virtual ~Frame();

  // FIXME: Merge these methods and move them into FrameLoader.
  virtual bool openURL(const KURL&);
  virtual void openURLRequest(const FrameLoadRequest&) = 0;
  void changeLocation(const DeprecatedString& URL, const String& referrer, bool lockHistory = true, bool userGesture = false);
  virtual void urlSelected(const ResourceRequest&, const String& target, const Event* triggeringEvent, bool lockHistory = false);
  virtual void urlSelected(const FrameLoadRequest&, const Event* triggeringEvent) = 0;
  
  FrameLoader* frameLoader();

  bool requestFrame(Element* ownerElement, const String& url, const AtomicString& frameName);
  virtual Frame* createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer) = 0;
  Frame* loadSubframe(Element* ownerElement, const KURL& url, const String& name, const String& referrer);

  virtual void submitForm(const FrameLoadRequest&) = 0;
  void submitForm(const char* action, const String& url, const FormData& formData,
                  const String& target, const String& contentType = String(),
                  const String& boundary = String());
  void submitFormAgain();

  void stop();
  void stopLoading(bool sendUnload = false);
  virtual bool closeURL();
private:
  void cancelRedirection(bool newLoadInProgress = false);

public:
  void didExplicitOpen();

  KURL iconURL();
  void commitIconURLToIconDatabase();
  
  Page* page() const;
  void pageDestroyed();
  
  /**
   * Returns a pointer to the @ref BrowserExtension.
   */
  BrowserExtension* browserExtension() const;

  /**
   * Returns a pointer to the HTML document's view.
   */
  FrameView* view() const;

  virtual void setView(FrameView*);

  /**
   * Returns @p true if Javascript is enabled, @p false otherwise.
   */
  bool jScriptEnabled() const;

  /**
   * Returns true if Java is enabled, false otherwise.
   */
  bool javaEnabled() const;
  
  /**
   * Returns true if plugins are enabled, false otherwise.
   */
  bool pluginsEnabled() const;

  /**
   * Execute the specified snippet of JavaScript code.
   */
  KJS::JSValue* executeScript(Node*, const String& script, bool forceUserGesture = false);

  /**
   * Implementation of CSS property -webkit-user-drag == auto
   */
  virtual bool shouldDragAutoNode(Node*, const IntPoint&) const;
  
  /**
   * Specifies whether images contained in the document should be loaded
   * automatically or not.
   *
   * @note Request will be ignored if called before @ref begin().
   */
  void setAutoLoadImages(bool enable);
  /**
   * Returns whether images contained in the document are loaded automatically
   * or not.
   * @note that the returned information is unreliable as long as no begin()
   * was called.
   */
  bool autoLoadImages() const;

  KURL baseURL() const;
  String baseTarget() const;

  /**
   * Schedules a redirection after @p delay seconds.
   */
  void scheduleRedirection(double delay, const DeprecatedString& url, bool lockHistory = true);

  /**
   * Make a location change, or schedule one for later.
   * These are used for JavaScript-triggered location changes.
   */
  void scheduleLocationChange(const DeprecatedString& url, const String& referrer, bool lockHistory = true, bool userGesture = false);
  void scheduleRefresh(bool userGesture = false);
  bool isScheduledLocationChangePending() const;

  /**
   * Schedules a history navigation operation (go forward, go back, etc.).
   * This is used for JavaScript-triggered location changes.
   */
  void scheduleHistoryNavigation(int steps);

  virtual bool canGoBackOrForward(int distance) const = 0;
  virtual void goBackOrForward(int distance) = 0;
  virtual int getHistoryLength() = 0;
  virtual KURL historyURL(int distance) = 0;

  /**
   * Clears the widget and prepares it for new content.
   *
   * If you want @ref url() to return
   * for example "file:/tmp/test.html", you can use the following code:
   * <PRE>
   * view->begin(KURL("file:/tmp/test.html"));
   * </PRE>
   *
   * @param url is the url of the document to be displayed.  Even if you
   * are generating the HTML on the fly, it may be useful to specify
   * a directory so that any images are found.
   *
   * All child frames and the old document are removed if you call
   * this method.
   */
  virtual void begin(const KURL& url = KURL());

  /**
   * Writes another part of the HTML code to the widget.
   *
   * You may call
   * this function many times in sequence. But remember: The fewer calls
   * you make, the faster the widget will be.
   *
   * The HTML code is sent through a decoder which decodes the stream to
   * Unicode.
   *
   * The @p len parameter is needed for streams encoded in utf-16,
   * since these can have \0 chars in them. In case the encoding
   * you're using isn't utf-16, you can safely leave out the length
   * parameter.
   *
   * Attention: Don't mix calls to @ref write(const char*) with calls
   * to @ref write(const String&).
   *
   * The result might not be what you want.
   */
  virtual void write(const char* str, int len = -1);

  /**
   * Writes another part of the HTML code to the widget.
   *
   * You may call
   * this function many times in sequence. But remember: The fewer calls
   * you make, the faster the widget will be.
   */
  virtual void write(const String&);

  /**
   * Call this after your last call to @ref write().
   */
  virtual void end();

  void endIfNotLoading();

  void paint(GraphicsContext*, const IntRect&);

  void setEncoding(const String& encoding, bool userChosen);
  String encoding() const;

  /**
   * Sets a user defined style sheet to be used on top of the HTML4,
   * SVG and printing default style sheets.
   */
  void setUserStyleSheetLocation(const KURL&);
  void setUserStyleSheet(const String& styleSheetData);
  
  /**
   * Sets the standard font style.
   *
   * @param name The font name to use for standard text.
   */
  void setStandardFont(const String& name);

  /**
   * Sets the fixed font style.
   *
   * @param name The font name to use for fixed text, e.g.
   * the <tt>&lt;pre&gt;</tt> tag.
   */
  void setFixedFont(const String& name);

  /**
   * Finds the anchor named @p name.
   *
   * If the anchor is found, the widget
   * scrolls to the closest position. Returns @p if the anchor has
   * been found.
   */
  bool gotoAnchor(const String& name);

  /**
   * Sets the Zoom factor. The value is given in percent, larger values mean a
   * generally larger font and larger page contents. It is not guaranteed that
   * all parts of the page are scaled with the same factor though.
   *
   * The given value should be in the range of 20..300, values outside that
   * range are not guaranteed to work. A value of 100 will disable all zooming
   * and show the page with the sizes determined via the given lengths in the
   * stylesheets.
   */
  void setZoomFactor(int percent);

  /**
   * Returns the current zoom factor.
   */
  int zoomFactor() const;

  /**
   * Returns the text the user has marked.
   */
  virtual String selectedText() const;
  
  bool findString(const String&, bool, bool, bool);

  /**
   * Returns the granularity of the selection (character, word, line, paragraph).
   */
  TextGranularity selectionGranularity() const;
  
  /**
   * Sets the granularity of the selection (character, word, line, paragraph).
   */
  void setSelectionGranularity(TextGranularity granularity) const;

  // FIXME: Replace these with functions on the selection controller.
  bool shouldChangeSelection(const Selection&) const;
  virtual bool shouldDeleteSelection(const Selection&) const;
  void clearCaretRectIfNeeded();
  void setFocusNodeIfNeeded();
  void selectionLayoutChanged();
  void notifyRendererOfSelectionChange(bool userTriggered);

  /**
   * Returns a mark, to be used as emacs uses it.
   */
  const Selection& mark() const;
  void setMark(const Selection&);
  
  /**
   * Transposes characters either side of caret selection.
   */
  void transpose();
  
  /**
   * Invalidates the current selection.
   */
  void invalidateSelection();

  void setCaretVisible(bool flag = true);
  void paintCaret(GraphicsContext*, const IntRect&) const;  
  void paintDragCaret(GraphicsContext*, const IntRect&) const;

  /**
   * Set info for vertical arrow navigation.
   */
  void setXPosForVerticalArrowNavigation(int x);

  /**
   * Get info for vertical arrow navigation.
   */
  int xPosForVerticalArrowNavigation() const;

  /**
   * Has the user selected anything?
   *
   *  Call @ref selectedText() to
   * retrieve the selected text.
   *
   * @return @p true if there is text selected.
   */
  bool hasSelection() const;

  /**
   * Marks all text in the document as selected.
   */
  void selectAll();

  /**
   * Marks contents of node as selected.
   * Returns whether the selection changed.
   */
  bool selectContentsOfNode(Node*);
 
  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldBeginEditing(const Range*) const;

  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldEndEditing(const Range*) const;

  /**
   * Called when editing has begun.
   */
  virtual void didBeginEditing() const {};
   
  /**
   * Called when editing has ended.
   */
  virtual void didEndEditing() const {};
    
  /**
   * Returns the contentEditable "override" value for the part
   */
  virtual bool isContentEditable() const;

  virtual void textFieldDidBeginEditing(Element*);
  virtual void textFieldDidEndEditing(Element*);
  virtual void textDidChangeInTextField(Element*);
  virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
  virtual void textWillBeDeletedInTextField(Element* input);
  virtual void textDidChangeInTextArea(Element*);

  virtual bool inputManagerHasMarkedText() const { return false; }
  
  virtual void setSecureKeyboardEntry(bool) {};
  virtual bool isSecureKeyboardEntry() { return false; }
  
  bool isSelectionInPasswordField();
  
  /**
   * Returns the most recent edit command applied.
   */
  EditCommand* lastEditCommand();

  /**
   * Called when editing has been applied.
   */
  void appliedEditing(PassRefPtr<EditCommand>);

  /**
   * Called when editing has been unapplied.
   */
  void unappliedEditing(PassRefPtr<EditCommand>);

  /**
   * Called when editing has been reapplied.
   */
  void reappliedEditing(PassRefPtr<EditCommand>);

  /**
   * Returns the typing style for the document.
   */
  CSSMutableStyleDeclaration* typingStyle() const;

  /**
   * Sets the typing style for the document.
   */
  void setTypingStyle(CSSMutableStyleDeclaration*);

  /**
   * Clears the typing style for the document.
   */
  void clearTypingStyle();

  virtual void tokenizerProcessedData() {}

  bool inViewSourceMode() const;
  void setInViewSourceMode(bool = true) const;

  const Settings* settings() const;

  void setJSStatusBarText(const String&);
  void setJSDefaultStatusBarText(const String&);
  String jsStatusBarText() const;
  String jsDefaultStatusBarText() const;

  /**
   * Referrer used for links in this page.
   */
  String referrer() const;

  /**
   * Last-modified date (in raw string format), if received in the [HTTP] headers.
   */
  String lastModified() const;

  bool isPointInsideSelection(const IntPoint&);

  virtual bool tabsToLinks() const;
  virtual bool tabsToAllControls() const;

  // Editing operations.
  enum TriState { falseTriState, trueTriState, mixedTriState };
  void copyToPasteboard();
  void cutToPasteboard();
  void pasteFromPasteboard();
  void pasteAndMatchStyle();
  bool mayCopy();
  virtual bool canPaste() const = 0;
  void redo();
  void undo();
  virtual bool canRedo() const = 0;
  virtual bool canUndo() const = 0;
  void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  void applyStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  void applyParagraphStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  void indent();
  void outdent();
  TriState selectionHasStyle(CSSStyleDeclaration*) const;
  bool selectionStartHasStyle(CSSStyleDeclaration*) const;
  String selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(Element*) const;
  void removeEditingStyleFromElement(Element*) const;
  virtual void print() = 0;
  virtual bool isCharacterSmartReplaceExempt(UChar, bool);

  // Used to keep the part alive when running a script that might destroy it.
  void keepAlive();

  static void endAllLifeSupport();

  /**
   * returns a KURL object for the given url. Use when
   * you know what you're doing.
   */
  KURL completeURL(const DeprecatedString& url);

  virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
  virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
  virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
  
  void updateSelectionForMouseDragOverPosition(const VisiblePosition&);

  void selectClosestWordFromMouseEvent(const PlatformMouseEvent&, Node* innerNode);

  // Methods with platform-specific overrides (and no base class implementation).
  virtual void setTitle(const String&) = 0;
  virtual void handledOnloadEvents() = 0;
  virtual String userAgent() const = 0;
  virtual String incomingReferrer() const = 0;
  virtual String mimeTypeForFileName(const String&) const = 0;
  virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::RootObject* bindingRootObject() = 0;
  virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args) = 0;
  
  virtual void markMisspellingsInAdjacentWords(const VisiblePosition&) = 0;
  virtual void markMisspellings(const Selection&) = 0;
  virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID) = 0;
  virtual void runJavaScriptAlert(const String& message) = 0;
  virtual bool runJavaScriptConfirm(const String& message) = 0;
  virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result) = 0;  
  virtual bool shouldInterruptJavaScript() = 0;
  virtual bool locationbarVisible() = 0;
  virtual bool menubarVisible() = 0;
  virtual bool personalbarVisible() = 0;
  virtual bool statusbarVisible() = 0;
  virtual bool toolbarVisible() = 0;
  virtual void scheduleClose() = 0;
  virtual void focusWindow() = 0;
  virtual void unfocusWindow() = 0;
  virtual void createEmptyDocument() = 0;
  virtual Range* markedTextRange() const = 0;
  virtual void registerCommandForUndo(PassRefPtr<EditCommand>) = 0;
  virtual void registerCommandForRedo(PassRefPtr<EditCommand>) = 0;
  virtual void clearUndoRedoOperations() = 0;
  virtual void issueUndoCommand() = 0;
  virtual void issueRedoCommand() = 0;
  virtual void issueCutCommand() = 0;
  virtual void issueCopyCommand() = 0;
  virtual void issuePasteCommand() = 0;
  virtual void issuePasteAndMatchStyleCommand() = 0;
  virtual void issueTransposeCommand() = 0;
  virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping) = 0;
  virtual void respondToChangedContents(const Selection& endingSelection) = 0;
  virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const = 0;
  virtual void partClearedInBegin() = 0; 
  virtual void saveDocumentState() = 0;
  virtual void restoreDocumentState() = 0;
  virtual bool lastEventIsMouseUp() const = 0;
  virtual String overrideMediaType() const = 0;
  virtual void redirectDataToPlugin(Widget* pluginWidget) { }
protected:
  virtual Plugin* createPlugin(Element* node, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType) = 0;
  virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType) = 0;

  virtual void redirectionTimerFired(Timer<Frame>*);
  
  virtual bool isLoadTypeReload() = 0;
  virtual KURL originalRequestURL() const = 0;

public:
  void loadDone();

  void finishedParsing();

  void checkCompleted();

  void reparseConfiguration();

private:
  void childBegin();

  void started();

  void completed(bool);
  void childCompleted(bool);
  void parentCompleted();

    void lifeSupportTimerFired(Timer<Frame>*);
    void endLifeSupport();

  virtual void clear(bool clearWindowProperties = true);

    void caretBlinkTimerFired(Timer<Frame>*);

  bool shouldUsePlugin(Node* element, const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback);
  bool loadPlugin(RenderPart* renderer, const KURL& url, const String& mimeType, 
                  const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback);

public:
  bool requestObject(RenderPart* frame, const String& url, const AtomicString& frameName,
                     const String& serviceType, const Vector<String>& paramNames, const Vector<String>& paramValues);
  Document* document() const;
  void setDocument(Document* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  virtual bool mouseDownMayStartSelect() const { return true; }
  bool mouseDownMayStartAutoscroll() const;
  void setMouseDownMayStartAutoscroll(bool b);

  bool mouseDownMayStartDrag() const;
  void setMouseDownMayStartDrag(bool b);
  
  void handleFallbackContent();

 public:
  KJS::JSValue* executeScript(const String& filename, int baseLine, Node*, const String& script);
  KJSProxy* jScript();
  Frame* opener();
  void setOpener(Frame* _opener);
  bool openedByJS();
  void setOpenedByJS(bool _openedByJS);

  void setSettings(Settings*);

  void provisionalLoadStarted();
  bool userGestureHint();
  void didNotOpenURL(const KURL&);
  void addData(const char* bytes, int length);
  void addMetaData(const String& key, const String& value);
  void setMediaType(const String&);

  // root renderer for the document contained in this frame
  RenderObject* renderer() const;
  
  Element* ownerElement();
  // renderer for the element that contains this frame
  RenderPart* ownerRenderer();

  IntRect selectionRect() const;
  FloatRect visibleSelectionRect() const;
  bool isFrameSet() const;

  HTMLFormElement* currentForm() const;

  RenderStyle* styleForSelectionStart(Node* &nodeToRemove) const;

  void revealSelection(const RenderLayer::ScrollAlignment& alignment = RenderLayer::gAlignCenterIfNeeded) const;
  void revealCaret(const RenderLayer::ScrollAlignment& alignment = RenderLayer::gAlignCenterIfNeeded) const;
  void setSelectionFromNone();

  bool scrollOverflow(ScrollDirection direction, ScrollGranularity granularity);

  void adjustPageHeight(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

  bool canCachePage();
  KJS::PausedTimeouts* pauseTimeouts();
  void resumeTimeouts(KJS::PausedTimeouts*);
  void saveWindowProperties(KJS::SavedProperties* windowProperties);
  void saveLocationProperties(KJS::SavedProperties* locationProperties);
  void restoreWindowProperties(KJS::SavedProperties* windowProperties);
  void restoreLocationProperties(KJS::SavedProperties* locationProperties);
  void saveInterpreterBuiltins(KJS::SavedBuiltins& interpreterBuiltins);
  void restoreInterpreterBuiltins(const KJS::SavedBuiltins& interpreterBuiltins);

  static Frame* frameForWidget(const Widget*);
  static Node* nodeForWidget(const Widget*);
  static Frame* frameForNode(Node*);

  static void clearDocumentFocus(Widget*);

  void updatePolicyBaseURL();
  void setPolicyBaseURL(const String&);

  void forceLayout();
  void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

  void sendResizeEvent();
  void sendScrollEvent();
  bool scrollbarsVisible();
  void scrollToAnchor(const KURL&);
  bool canMouseDownStartSelect(Node*);

  void clearTimers();
  static void clearTimers(FrameView*);

  bool isActive() const;
  virtual void setIsActive(bool flag);
  void setWindowHasFocus(bool flag);
  // Convenience, to avoid repeating the code to dig down to get this.

  UChar backslashAsCurrencySymbol() const;

  const Vector<MarkedTextUnderline>& markedTextUnderlines() const;  
  bool markedTextUsesUnderlines() const;
  
  unsigned markAllMatchesForText(const String&, bool caseFlag, unsigned limit);
  bool markedTextMatchesAreHighlighted() const;
  void setMarkedTextMatchesAreHighlighted(bool flag);
  
  // Call this method before handling a new user action, like on a mouse down or key down.
  // Currently, all this does is clear the "don't submit form twice" data member.
  void prepareForUserAction();
  Node* mousePressNode();
  
  void clearRecordedFormValues();
  void recordFormValue(const String& name, const String& value, PassRefPtr<HTMLFormElement>);

  bool isComplete() const;
  bool isLoadingMainResource() const;
  
  void replaceContentsWithScriptResult(const KURL& url);

  void disconnectOwnerElement();

  void setNeedsReapplyStyles();

  void stopAutoscrollTimer();
  RenderObject* autoscrollRenderer() const;

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();
    virtual void cleanupPluginObjects() { }
    void cancelAndClear();
    
    void handleAutoscroll(RenderObject*);
    void startAutoscrollTimer();
    void setAutoscrollRenderer(RenderObject*);

 private:
  void emitLoadEvent();
  
  void receivedFirstData();

  /**
   * @internal Extracts anchor and tries both encoded and decoded form.
   */
  void gotoAnchor();

  void handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
  void handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
  void handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);

  CSSComputedStyleDeclaration* selectionComputedStyle(Node* &nodeToRemove) const;

    virtual void setStatusBarText(const String&);
    
    void autoscrollTimerFired(Timer<Frame>*);

public:
  friend class FrameMac;
  friend class FrameWin;
#if PLATFORM(GDK)
  friend class FrameGdk;
#endif
#if PLATFORM(QT)
  friend class FrameQt;
#endif

  RenderObject::NodeInfo nodeInfoAtPoint(const IntPoint&, bool allowShadowContent);
  bool hasSelection();
  String documentTypeString() const;

  void checkEmitLoadEvent();
  bool didOpenURL(const KURL&);
  virtual void didFirstLayout() {}

  virtual void frameDetached();

  void detachChildren();

  void updateBaseURLForEmptyDocument();

  KURL url() const;
  
  void setResponseMIMEType(const String&);
  const String& responseMIMEType() const;
  
  bool containsPlugins() const;
  
  bool prohibitsScrolling() const;
  void setProhibitsScrolling(const bool);
  
  // split out controller objects
  FrameTree* tree() const;
  SelectionController* selectionController() const;
  SelectionController* dragCaretController() const;
  DOMWindow* domWindow() const;
  Editor* editor() const;
  CommandByName* command() const;
 private:
  friend class FramePrivate;
  FramePrivate* d;
};

} // namespace WebCore

#endif // Frame_H
