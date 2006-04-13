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

#include "BrowserExtension.h"
#include "Color.h"
#include "DeprecatedStringList.h"
#include "EditAction.h"
#include "FrameView.h"
#include "KWQScrollBar.h"
#include "Node.h"
#include "TextAffinity.h"
#include "TextGranularity.h"
#include "TransferJobClient.h"
#include <kxmlcore/Vector.h>

class KHTMLSettings;

namespace KJS {
    class JSValue;
    class PausedTimeouts;
    class SavedBuiltins;
    class SavedProperties;

    namespace Bindings {
        class Instance;
    }
}

namespace WebCore {

class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class CSSStyleDeclaration;
class DrawContentsEvent;
class EditCommandPtr;
class FramePrivate;
class FrameTree;
class KJSProxy;
class Page;
class Plugin;
class MouseEventWithHitTestResults;
class Range;
class RenderLayer;
class Selection;
class SelectionController;
class VisiblePosition;

template <typename T> class Timer;

struct MarkedTextUnderline {
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
    ObjectContentPlugin,
};

class Frame : public Shared<Frame>, Noncopyable, TransferJobClient {

public:
  enum { NoXPosForVerticalArrowNavigation = INT_MIN };

  Frame(Page*, RenderPart*);
  virtual ~Frame();

  virtual bool openURL(const KURL&);
  virtual bool closeURL();

  void didExplicitOpen();

  Page* page() const;

  /**
   * Stop loading the document and kill all data requests (for images, etc.)
   */
  void stopLoading(bool sendUnload = false);

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
  KJS::JSValue* executeScript(Node*, const DeprecatedString& script, bool forceUserGesture = false);

  /**
   * Implementation of CSS property -khtml-user-drag == auto
   */
  virtual bool shouldDragAutoNode(Node*, const IntPoint&) const;
  
  /**
   * Specifies whether images contained in the document should be loaded
   * automatically or not.
   *
   * @note Request will be ignored if called before @ref begin().
   */
  void setAutoloadImages(bool enable);
  /**
   * Returns whether images contained in the document are loaded automatically
   * or not.
   * @note that the returned information is unreliable as long as no begin()
   * was called.
   */
  bool autoloadImages() const;

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
  void changeLocation(const DeprecatedString& URL, const DeprecatedString& referrer, bool lockHistory = true, bool userGesture = false);
  void scheduleLocationChange(const DeprecatedString& url, const DeprecatedString& referrer, bool lockHistory = true, bool userGesture = false);
  bool isScheduledLocationChangePending() const;

  /**
   * Schedules a history navigation operation (go forward, go back, etc.).
   * This is used for JavaScript-triggered location changes.
   */
  void scheduleHistoryNavigation(int steps);

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
   * The HTML code is send through a decoder which decodes the stream to
   * Unicode.
   *
   * The @p len parameter is needed for streams encoded in utf-16,
   * since these can have \0 chars in them. In case the encoding
   * you're using isn't utf-16, you can safely leave out the length
   * parameter.
   *
   * Attention: Don't mix calls to @ref write(const char*) with calls
   * to @ref write(const DeprecatedString&  ).
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
  virtual void write(const DeprecatedString& str);

  /**
   * Call this after your last call to @ref write().
   */
  virtual void end();

  void endIfNotLoading();

  /**
   * Similar to end, but called to abort a load rather than cleanly end.
   */
  void stop();

  void paint(GraphicsContext*, const IntRect&);

  void setEncoding(const DeprecatedString& encoding, bool userChosen);

  /**
   * Returns the encoding the page currently uses.
   *
   * Note that the encoding might be different from the charset.
   */
  DeprecatedString encoding() const;

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

  /**
   * Returns the granularity of the selection (character, word, line, paragraph).
   */
  TextGranularity selectionGranularity() const;
  
  /**
   * Sets the granularity of the selection (character, word, line, paragraph).
   */
  void setSelectionGranularity(TextGranularity granularity) const;

  /**
   * Returns the drag caret of the HTML.
   */
  const SelectionController& dragCaret() const;

  /**
   * Sets the current selection.
   */
  void setSelection(const SelectionController&, bool closeTyping = true, bool keepTypingStyle = false);

  /**
   * Returns whether selection can be changed.
   */
  bool shouldChangeSelection(const SelectionController&) const;

  /**
   * Returns a mark, to be used as emacs uses it.
   */
  const Selection& mark() const;

  /**
   * Returns the mark.
   */
  void setMark(const Selection&);

  /**
   * Sets the current drag caret.
   */
  void setDragCaret(const SelectionController&);
  
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

  /**
   * Returns the most recent edit command applied.
   */
  EditCommandPtr lastEditCommand();

  /**
   * Called when editing has been applied.
   */
  void appliedEditing(EditCommandPtr&);

  /**
   * Called when editing has been unapplied.
   */
  void unappliedEditing(EditCommandPtr&);

  /**
   * Called when editing has been reapplied.
   */
  void reappliedEditing(EditCommandPtr&);

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

  const KHTMLSettings* settings() const;

  void setJSStatusBarText(const String&);
  void setJSDefaultStatusBarText(const String&);
  String jsStatusBarText() const;
  String jsDefaultStatusBarText() const;

  /**
   * Referrer used for links in this page.
   */
  DeprecatedString referrer() const;

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
  virtual bool canPaste() const = 0;
  void redo();
  void undo();
  virtual bool canRedo() const = 0;
  virtual bool canUndo() const = 0;
  void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  void applyStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  void applyParagraphStyle(CSSStyleDeclaration* , EditAction editingAction=EditActionUnspecified);
  TriState selectionHasStyle(CSSStyleDeclaration*) const;
  bool selectionStartHasStyle(CSSStyleDeclaration*) const;
  String selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(Element*) const;
  void removeEditingStyleFromElement(Element*) const;
  virtual void print() = 0;
  virtual bool isCharacterSmartReplaceExempt(const QChar&, bool);

  // Used to keep the part alive when running a script that might destroy it.
  void keepAlive();

  static void endAllLifeSupport();

  /**
   * returns a KURL object for the given url. Use when
   * you know what you're doing.
   */
  KURL completeURL(const DeprecatedString& url);

  virtual void handleMouseReleaseDoubleClickEvent(const MouseEventWithHitTestResults&);
  virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
  virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
  virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
  
  void selectClosestWordFromMouseEvent(const PlatformMouseEvent&, Node* innerNode);

  virtual void urlSelected(const DeprecatedString& url, const String& target);
  virtual void urlSelected(const ResourceRequest&, const String& target);


  // Methods with platform-specific overrides (and no base class implementation).
  virtual void setTitle(const String&) = 0;
  virtual void handledOnloadEvents() = 0;
  virtual String userAgent() const = 0;
  virtual String incomingReferrer() const = 0;
  virtual String mimeTypeForFileName(const String&) const = 0;
  virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*) = 0;
  virtual void markMisspellingsInAdjacentWords(const VisiblePosition&) = 0;
  virtual void markMisspellings(const SelectionController&) = 0;
  virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID) = 0;
  virtual void runJavaScriptAlert(const String& message) = 0;
  virtual bool runJavaScriptConfirm(const String& message) = 0;
  virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result) = 0;  
  virtual bool locationbarVisible() = 0;
  virtual bool menubarVisible() = 0;
  virtual bool personalbarVisible() = 0;
  virtual bool statusbarVisible() = 0;
  virtual bool toolbarVisible() = 0;
  virtual void scheduleClose() = 0;
  virtual void unfocusWindow() = 0;
  virtual void createEmptyDocument() = 0;
  virtual Range* markedTextRange() const = 0;
  virtual void registerCommandForUndo(const EditCommandPtr&) = 0;
  virtual void registerCommandForRedo(const EditCommandPtr&) = 0;
  virtual void clearUndoRedoOperations() = 0;
  virtual void issueUndoCommand() = 0;
  virtual void issueRedoCommand() = 0;
  virtual void issueCutCommand() = 0;
  virtual void issueCopyCommand() = 0;
  virtual void issuePasteCommand() = 0;
  virtual void issuePasteAndMatchStyleCommand() = 0;
  virtual void issueTransposeCommand() = 0;
  virtual void respondToChangedSelection(const SelectionController& oldSelection, bool closeTyping) = 0;
  virtual void respondToChangedContents() = 0;
  virtual bool shouldChangeSelection(const SelectionController& oldSelection, const SelectionController& newSelection, EAffinity affinity, bool stillSelecting) const = 0;
  virtual void partClearedInBegin() = 0; 
  virtual void saveDocumentState() = 0;
  virtual void restoreDocumentState() = 0;
  virtual bool canGoBackOrForward(int distance) const = 0;
  virtual void openURLRequest(const ResourceRequest&) = 0;
  virtual void submitForm(const ResourceRequest&) = 0;
  virtual void urlSelected(const ResourceRequest&) = 0;
  virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframePart = 0) = 0;
  virtual bool passWheelEventToChildWidget(Node*) = 0;
  virtual bool lastEventIsMouseUp() const = 0;
  virtual String overrideMediaType() const = 0;
protected:
  virtual Plugin* createPlugin(const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType) = 0;
  virtual Frame* createFrame(const KURL& url, const String& name, RenderPart* renderer, const String& referrer) = 0;
  virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType) = 0;

    virtual void redirectionTimerFired(Timer<Frame>*);

public:
  void loadDone();

  void finishedParsing();

  void checkCompleted();

  void reparseConfiguration();

private:
    virtual void receivedRedirect(TransferJob*, const KURL&);
    virtual void receivedAllData(TransferJob*);

  void childBegin();

  void submitFormAgain();

  void started();

  void completed(bool);
  void childCompleted(bool);
  void parentCompleted();

    void lifeSupportTimerFired(Timer<Frame>*);
    void endLifeSupport();

  virtual void clear(bool clearWindowProperties = true);

  void clearCaretRectIfNeeded();
  void setFocusNodeIfNeeded();
  void selectionLayoutChanged();
    void caretBlinkTimerFired(Timer<Frame>*);

  bool shouldUsePlugin(Node* element, const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback);
  bool loadPlugin(RenderPart* renderer, const KURL& url, const String& mimeType, 
                  const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback);
  Frame* loadSubframe(RenderPart* renderer, const KURL& url, const String& name, const String& referrer);

public:
  void submitForm(const char* action, const String& url, const FormData& formData,
                  const String& target, const String& contentType = String(),
                  const String& boundary = String());
  
  bool requestObject(RenderPart* frame, const String& url, const AtomicString& frameName,
                     const String& serviceType, const Vector<String>& paramNames, const Vector<String>& paramValues);
  bool requestFrame(RenderPart* frame, const String& url, const AtomicString& frameName);

  Document* document() const;
  void setDocument(Document* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  virtual bool mouseDownMayStartSelect() const { return true; }

  void handleFallbackContent();

private:
  void cancelRedirection(bool newLoadInProgress = false);

 public:
  KJS::JSValue* executeScript(const String& filename, int baseLine, Node*, const DeprecatedString& script);
  KJSProxy* jScript();
  Frame* opener();
  void setOpener(Frame* _opener);
  bool openedByJS();
  void setOpenedByJS(bool _openedByJS);

  void setSettings(KHTMLSettings*);

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

  // Scrolls as necessary to reveal the selection
  void revealSelection();
  // Centers the selection regardless of whether it was already visible
  void centerSelectionInVisibleArea() const;
  void setSelectionFromNone();

  bool scrollOverflow(KWQScrollDirection direction, KWQScrollGranularity granularity);

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
  bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&, bool isDoubleClick);
  bool passWidgetMouseDownEventToWidget(RenderWidget*);
  virtual bool passMouseDownEventToWidget(Widget*) = 0;

  void clearTimers();
  static void clearTimers(FrameView*);

  bool displaysWithFocusAttributes() const;
  virtual void setDisplaysWithFocusAttributes(bool flag);
  void setWindowHasFocus(bool flag);
  // Convenience, to avoid repeating the code to dig down to get this.

  QChar backslashAsCurrencySymbol() const;

  DeprecatedValueList<MarkedTextUnderline> markedTextUnderlines() const;  
  bool markedTextUsesUnderlines() const;
  unsigned highlightAllMatchesForString(const String&, bool caseFlag);

  // Call this method before handling a new user action, like on a mouse down or key down.
  // Currently, all this does is clear the "don't submit form twice" data member.
  void prepareForUserAction();
  Node* mousePressNode();
  
  void clearRecordedFormValues();
  void recordFormValue(const String& name, const String& value, PassRefPtr<HTMLFormElement>);

  bool isComplete() const;
  bool isLoadingMainResource() const;
  
  void replaceContentsWithScriptResult(const KURL& url);

    void disconnectOwnerRenderer();

    void setNeedsReapplyStyles();

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();
    
    void handleAutoscroll(RenderLayer*);
    void startAutoscrollTimer();
    void stopAutoscrollTimer();

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

  void checkEmitLoadEvent();
  bool didOpenURL(const KURL&);
  virtual void didFirstLayout() {}

  virtual void frameDetached();

  void updateBaseURLForEmptyDocument();

  KURL url() const;
  void setResourceRequest(const ResourceRequest& request);
  const ResourceRequest& resourceRequest() const;

  // split out controller objects
  FrameTree* tree() const;
  SelectionController& selection() const;

 private:
  friend class FramePrivate;
  FramePrivate* d;
};

}

#endif
