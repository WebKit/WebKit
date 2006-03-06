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

#ifndef FRAME_H
#define FRAME_H

#include "BrowserExtension.h"
#include "Color.h"
#include "FrameView.h"
#include "NodeImpl.h"
#include "Shared.h"
#include "TransferJobClient.h"
#include "edit_actions.h"
#include "text_affinity.h"
#include "text_granularity.h"
#include <KURL.h>
#include <qscrollbar.h>
#include <QStringList.h>
#include <kxmlcore/Noncopyable.h>

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

class CSSComputedStyleDeclarationImpl;
class CSSMutableStyleDeclarationImpl;
class CSSStyleDeclarationImpl;
class DrawContentsEvent;
class EditCommandPtr;
class FramePrivate;
class FrameTree;
class KJSProxyImpl;
class Page;
class Plugin;
class MouseEventWithHitTestResults;
class RangeImpl;
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
  BrowserExtension *browserExtension() const;

  /**
   * Returns a pointer to the HTML document's view.
   */
  FrameView *view() const;

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
   * Enable/disable the automatic forwarding by <meta http-equiv="refresh" ....>
   */
  void setMetaRefreshEnabled( bool enable );

  /**
   * Returns @p true if automtaic forwarding is enabled.
   */
  bool metaRefreshEnabled() const;

  /**
   * Execute the specified snippet of JavaScript code.
   */
  KJS::JSValue* executeScript(NodeImpl*, const QString& script, bool forceUserGesture = false);

  /**
   * Implementation of CSS property -khtml-user-drag == auto
   */
  virtual bool shouldDragAutoNode(NodeImpl*, int x, int y) const;
  
  /**
   * Specifies whether images contained in the document should be loaded
   * automatically or not.
   *
   * @note Request will be ignored if called before @ref begin().
   */
  void setAutoloadImages( bool enable );
  /**
   * Returns whether images contained in the document are loaded automatically
   * or not.
   * @note that the returned information is unreliable as long as no begin()
   * was called.
   */
  bool autoloadImages() const;

  void autoloadImages(bool e) { setAutoloadImages(e); }
  void enableMetaRefresh(bool e) { setMetaRefreshEnabled(e); }
  bool setCharset( const QString &, bool ) { return true; }

  KURL baseURL() const;
  QString baseTarget() const;

  /**
   * Returns the URL for the background Image (used by save background)
   */
  KURL backgroundURL() const;

  /**
   * Schedules a redirection after @p delay seconds.
   */
  void scheduleRedirection(double delay, const QString &url, bool lockHistory = true);

  /**
   * Make a location change, or schedule one for later.
   * These are used for JavaScript-triggered location changes.
   */
  void changeLocation(const QString &URL, const QString &referrer, bool lockHistory = true, bool userGesture = false);
  void scheduleLocationChange(const QString &url, const QString &referrer, bool lockHistory = true, bool userGesture = false);
  bool isScheduledLocationChangePending() const;

  /**
   * Schedules a history navigation operation (go forward, go back, etc.).
   * This is used for JavaScript-triggered location changes.
   */
  void scheduleHistoryNavigation( int steps );

  /**
   * Clears the widget and prepares it for new content.
   *
   * If you want @ref url() to return
   * for example "file:/tmp/test.html", you can use the following code:
   * <PRE>
   * view->begin( KURL("file:/tmp/test.html" ) );
   * </PRE>
   *
   * @param url is the url of the document to be displayed.  Even if you
   * are generating the HTML on the fly, it may be useful to specify
   * a directory so that any images are found.
   *
   * @param xOffset is the initial horizontal scrollbar value. Usually
   * you don't want to use this.
   *
   * @param yOffset is the initial vertical scrollbar value. Usually
   * you don't want to use this.
   *
   * All child frames and the old document are removed if you call
   * this method.
   */
  virtual void begin( const KURL &url = KURL(), int xOffset = 0, int yOffset = 0 );

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
   * Attention: Don't mix calls to @ref write( const char *) with calls
   * to @ref write( const QString & ).
   *
   * The result might not be what you want.
   */
  virtual void write( const char *str, int len = -1 );

  /**
   * Writes another part of the HTML code to the widget.
   *
   * You may call
   * this function many times in sequence. But remember: The fewer calls
   * you make, the faster the widget will be.
   */
  virtual void write( const QString &str );

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

  void setEncoding(const QString &encoding, bool userChosen);

  /**
   * Returns the encoding the page currently uses.
   *
   * Note that the encoding might be different from the charset.
   */
  QString encoding() const;

  /**
   * Sets a user defined style sheet to be used on top of the HTML 4
   * default style sheet.
   *
   * This gives a wide range of possibilities to
   * change the layout of the page.
   */
  void setUserStyleSheet(const KURL &url);

  /**
   * Sets a user defined style sheet to be used on top of the HTML 4
   * default style sheet.
   *
   * This gives a wide range of possibilities to
   * change the layout of the page.
   */
  void setUserStyleSheet(const QString &styleSheet);

  /**
   * Sets the standard font style.
   *
   * @param name The font name to use for standard text.
   */
  void setStandardFont( const QString &name );

  /**
   * Sets the fixed font style.
   *
   * @param name The font name to use for fixed text, e.g.
   * the <tt>&lt;pre&gt;</tt> tag.
   */
  void setFixedFont( const QString &name );

  /**
   * Finds the anchor named @p name.
   *
   * If the anchor is found, the widget
   * scrolls to the closest position. Returns @p if the anchor has
   * been found.
   */
  bool gotoAnchor( const QString &name );

  /**
   * Initiates a text search.
   */
  void findTextBegin(NodeImpl *startNode = 0, int startPos = -1);

  /**
   * Finds the next occurrence of the string or expression.
   * If isRegExp is true then str is converted to a QRegExp, and caseSensitive is ignored.
   */
  bool findTextNext( const QString &str, bool forward, bool caseSensitive, bool isRegExp );

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
  virtual QString selectedText() const;

  /**
   * Returns the granularity of the selection (character, word, line, paragraph).
   */
  ETextGranularity selectionGranularity() const;
  
  /**
   * Sets the granularity of the selection (character, word, line, paragraph).
   */
  void setSelectionGranularity(ETextGranularity granularity) const;

  /**
   * Returns the drag caret of the HTML.
   */
  const SelectionController &dragCaret() const;

  /**
   * Sets the current selection.
   */
  void setSelection(const SelectionController &, bool closeTyping = true, bool keepTypingStyle = false);

  /**
   * Returns whether selection can be changed.
   */
  bool shouldChangeSelection(const SelectionController &) const;

  /**
   * Returns a mark, to be used as emacs uses it.
   */
  const Selection &mark() const;

  /**
   * Returns the mark.
   */
  void setMark(const Selection &);

  /**
   * Sets the current drag caret.
   */
  void setDragCaret(const SelectionController &);
  
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
  bool selectContentsOfNode(NodeImpl*);
 
  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldBeginEditing(const RangeImpl *) const;

  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldEndEditing(const RangeImpl *) const;

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

  /**
   * Returns the most recent edit command applied.
   */
  EditCommandPtr lastEditCommand();

  /**
   * Called when editing has been applied.
   */
  void appliedEditing(EditCommandPtr &);

  /**
   * Called when editing has been unapplied.
   */
  void unappliedEditing(EditCommandPtr &);

  /**
   * Called when editing has been reapplied.
   */
  void reappliedEditing(EditCommandPtr &);

  /**
   * Returns the typing style for the document.
   */
  CSSMutableStyleDeclarationImpl *typingStyle() const;

  /**
   * Sets the typing style for the document.
   */
  void setTypingStyle(CSSMutableStyleDeclarationImpl *);

  /**
   * Clears the typing style for the document.
   */
  void clearTypingStyle();

  virtual void tokenizerProcessedData() {};

  const KHTMLSettings *settings() const;

  /**
   * Returns a list of names of all frame (including iframe) objects of
   * the current document. Note that this method is not working recursively
   * for sub-frames.
   */
  QStringList frameNames() const;

  QPtrList<Frame> frames() const;

  Frame *childFrameNamed(const QString &name) const;

  /**
   * Finds a frame by name. Returns 0L if frame can't be found.
   */
  Frame *findFrame( const QString &f );

  /**
   * Return the current frame (the one that has focus)
   * Not necessarily a direct child of ours, framesets can be nested.
   * Returns "this" if this part isn't a frameset.
   */
  Frame* currentFrame() const;

  /**
   * Returns whether a frame with the specified name is exists or not.
   * In contrary to the @ref findFrame method this one also returns true
   * if the frame is defined but no displaying component has been
   * found/loaded, yet.
   */
  bool frameExists( const QString &frameName );

  void setJSStatusBarText(const String&);
  void setJSDefaultStatusBarText(const String&);
  String jsStatusBarText() const;
  String jsDefaultStatusBarText() const;

  /**
   * Referrer used for links in this page.
   */
  QString referrer() const;

  /**
   * Last-modified date (in raw string format), if received in the [HTTP] headers.
   */
  QString lastModified() const;

  /**
   * Loads into the cache.
   */
  void preloadStyleSheet(const QString &url, const QString &stylesheet);
  void preloadScript(const QString &url, const QString &script);

  bool isPointInsideSelection(int x, int y);

  virtual bool tabsToLinks() const;
  virtual bool tabsToAllControls() const;

  bool restored() const;

  void incrementFrameCount();
  void decrementFrameCount();
  int topLevelFrameCount();

  // Editing operations.
  // Not clear if these will be wanted in Frame by KDE,
  // but for now these bridge so we don't have to pepper the
  // KHTML code with WebCore-specific stuff.
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
  void computeAndSetTypingStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  void applyStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  void applyParagraphStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  TriState selectionHasStyle(CSSStyleDeclarationImpl *) const;
  bool selectionStartHasStyle(CSSStyleDeclarationImpl *) const;
  String selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(ElementImpl *) const;
  void removeEditingStyleFromElement(ElementImpl *) const;
  virtual void print() = 0;
  virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);

  // Used to keep the part alive when running a script that might destroy it.
  void keepAlive();

  static void endAllLifeSupport();

protected:
  /**
   * Emitted if the cursor is moved over an URL.
   */
  void onURL( const QString &url );

  /**
   * This signal is emitted when the selection changes.
   */
  void selectionChanged();

public:
  /**
   * returns a KURL object for the given url. Use when
   * you know what you're doing.
   */
  KURL completeURL( const QString &url );

  /**
   * presents a detailed error message to the user.
   * @p errorCode kio error code, eg KIO::ERR_SERVER_TIMEOUT.
   * @p text kio additional information text.
   * @p url the url that triggered the error.
   */
  void htmlError(int errorCode, const QString& text, const KURL& reqUrl);

  virtual void khtmlMouseDoubleClickEvent(MouseEventWithHitTestResults*);
  virtual void khtmlMousePressEvent(MouseEventWithHitTestResults*);
  virtual void khtmlMouseMoveEvent(MouseEventWithHitTestResults*);
  virtual void khtmlMouseReleaseEvent(MouseEventWithHitTestResults*);
  
  void selectClosestWordFromMouseEvent(MouseEvent*, NodeImpl* innerNode, int x, int y);

  virtual bool openFile();

  virtual void urlSelected(const QString& url, const QString& target, const URLArgs& args = URLArgs());


  // Methods with platform-specific overrides (and no base class implementation).
  virtual void setTitle(const String &) = 0;
  virtual void handledOnloadEvents() = 0;
  virtual QString userAgent() const = 0;
  virtual QString incomingReferrer() const = 0;
  virtual QString mimeTypeForFileName(const QString &) const = 0;
  virtual void clearRecordedFormValues() = 0;
  virtual void recordFormValue(const QString &name, const QString &value, HTMLFormElementImpl *element) = 0;
  virtual KJS::Bindings::Instance *getEmbedInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance *getObjectInstanceForWidget(Widget*) = 0;
  virtual KJS::Bindings::Instance *getAppletInstanceForWidget(Widget*) = 0;
  virtual void markMisspellingsInAdjacentWords(const VisiblePosition &) = 0;
  virtual void markMisspellings(const SelectionController &) = 0;
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
  virtual RangeImpl *markedTextRange() const = 0;
  virtual void registerCommandForUndo(const EditCommandPtr &) = 0;
  virtual void registerCommandForRedo(const EditCommandPtr &) = 0;
  virtual void clearUndoRedoOperations() = 0;
  virtual void issueUndoCommand() = 0;
  virtual void issueRedoCommand() = 0;
  virtual void issueCutCommand() = 0;
  virtual void issueCopyCommand() = 0;
  virtual void issuePasteCommand() = 0;
  virtual void issuePasteAndMatchStyleCommand() = 0;
  virtual void issueTransposeCommand() = 0;
  virtual void respondToChangedSelection(const SelectionController &oldSelection, bool closeTyping) = 0;
  virtual void respondToChangedContents() = 0;
  virtual bool shouldChangeSelection(const SelectionController &oldSelection, const SelectionController &newSelection, EAffinity affinity, bool stillSelecting) const = 0;
  virtual void partClearedInBegin() = 0; 
  virtual void saveDocumentState() = 0;
  virtual void restoreDocumentState() = 0;
  virtual bool canGoBackOrForward(int distance) const = 0;
  virtual void openURLRequest(const KURL &, const URLArgs &) = 0;
  virtual void submitForm(const KURL &, const URLArgs &) = 0;
  virtual void urlSelected(const KURL&, const URLArgs& args) = 0;
  virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults &) = 0;
  virtual bool passWheelEventToChildWidget(NodeImpl *) = 0;
  virtual bool lastEventIsMouseUp() const = 0;
  virtual QString overrideMediaType() const = 0;
protected:
  virtual String generateFrameName() = 0;
  virtual Plugin* createPlugin(const KURL& url, const QStringList& paramNames, const QStringList& paramValues, const QString& mimeType) = 0;
  virtual Frame* createFrame(const KURL& url, const QString& name, RenderPart* renderer, const String& referrer) = 0;
  virtual ObjectContentType objectContentType(const KURL& url, const QString& mimeType) = 0;

    virtual void redirectionTimerFired(Timer<Frame>*);

public:
  /**
   * Stops all animated images on the current and child pages
   */
  void stopAnimations();

  void loadDone();

  void finishedParsing();

  void checkCompleted();

  void reparseConfiguration();

private:
    virtual void receivedRedirect(TransferJob*, const KURL&);
    virtual void receivedAllData(TransferJob*);

  void childBegin();

  void submitFormAgain();

  void updateActions();

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
  bool openURLInFrame( const KURL &url, const URLArgs &urlArgs );

  void overURL( const QString &url, const QString &target, bool shiftPressed = false );

  bool shouldUsePlugin(NodeImpl* element, const KURL& url, const QString& mimeType, bool hasFallback, bool& useFallback);
  bool loadPlugin(RenderPart* renderer, const KURL &url, const QString &mimeType, 
                  const QStringList& paramNames, const QStringList& paramValues, bool useFallback);
  Frame* loadSubframe(RenderPart* renderer, const KURL& url, const QString& name, const String& referrer);

public:
  void submitForm(const char *action, const QString &url, const FormData &formData,
                  const QString &target, const QString& contentType = QString::null,
                  const QString& boundary = QString::null );
  
  bool requestObject(RenderPart *frame, const QString &url, const QString &frameName,
                     const QString &serviceType, const QStringList &paramNames, const QStringList &paramValues);
  bool requestFrame(RenderPart *frame, const QString &url, const QString &frameName);
  String requestFrameName();

  DocumentImpl *document() const;
  void setDocument(DocumentImpl* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  virtual bool mouseDownMayStartSelect() const { return true; }

  void handleFallbackContent();

private:
  bool checkLinkSecurity(const KURL &linkURL,const QString &message = QString::null, const QString &button = QString::null);
  
  void cancelRedirection(bool newLoadInProgress = false);

 public:
  KJS::JSValue* executeScript(const QString& filename, int baseLine, NodeImpl*, const QString& script);
  KJSProxyImpl *jScript();
  Frame *opener();
  void setOpener(Frame *_opener);
  bool openedByJS();
  void setOpenedByJS(bool _openedByJS);

  void setSettings(KHTMLSettings *);

  void provisionalLoadStarted();
  bool userGestureHint();
  void didNotOpenURL(const KURL &);
  void addData(const char *bytes, int length);
  void addMetaData(const QString &key, const QString &value);
  void setMediaType(const QString &);

  // root renderer for the document contained in this frame
  RenderObject* renderer() const;
  
  ElementImpl* ownerElement();
  // renderer for the element that contains this frame
  RenderPart* ownerRenderer();

  IntRect selectionRect() const;
  bool isFrameSet() const;

  HTMLFormElementImpl *currentForm() const;

  RenderStyle *styleForSelectionStart(NodeImpl *&nodeToRemove) const;

  // Scrolls as necessary to reveal the selection
  void revealSelection();
  // Centers the selection regardless of whether it was already visible
  void centerSelectionInVisibleArea() const;
  void setSelectionFromNone();

  bool scrollOverflow(KWQScrollDirection direction, KWQScrollGranularity granularity);

  void adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float bottomLimit);

  bool canCachePage();
  KJS::PausedTimeouts *pauseTimeouts();
  void resumeTimeouts(KJS::PausedTimeouts *);
  void saveWindowProperties(KJS::SavedProperties *windowProperties);
  void saveLocationProperties(KJS::SavedProperties *locationProperties);
  void restoreWindowProperties(KJS::SavedProperties *windowProperties);
  void restoreLocationProperties(KJS::SavedProperties *locationProperties);
  void saveInterpreterBuiltins(KJS::SavedBuiltins &interpreterBuiltins);
  void restoreInterpreterBuiltins(const KJS::SavedBuiltins &interpreterBuiltins);

  static Frame *frameForWidget(const Widget *);
  static NodeImpl *nodeForWidget(const Widget *);
  static Frame *frameForNode(NodeImpl *);

  static void setDocumentFocus(Widget *);
  static void clearDocumentFocus(Widget *);

  static const QPtrList<Frame> &instances() { return mutableInstances(); }    
  static QPtrList<Frame> &mutableInstances();

  void updatePolicyBaseURL();
  void setPolicyBaseURL(const String&);

  void forceLayout();
  void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

  void sendResizeEvent();
  void sendScrollEvent();
  bool scrollbarsVisible();
  void scrollToAnchor(const KURL &);
  bool canMouseDownStartSelect(NodeImpl* node);
  bool passWidgetMouseDownEventToWidget(MouseEventWithHitTestResults *, bool isDoubleClick);
  bool passWidgetMouseDownEventToWidget(RenderWidget *);
  virtual bool passMouseDownEventToWidget(Widget *) = 0;

  void clearTimers();
  static void clearTimers(FrameView *);

  bool displaysWithFocusAttributes() const;
  void setWindowHasFocus(bool flag);
  // Convenience, to avoid repeating the code to dig down to get this.

  QChar backslashAsCurrencySymbol() const;

  QValueList<MarkedTextUnderline> markedTextUnderlines() const;  
  bool markedTextUsesUnderlines() const;
  unsigned highlightAllMatchesForString(const QString &, bool caseFlag);

  // Call this method before handling a new user action, like on a mouse down or key down.
  // Currently, all this does is clear the "don't submit form twice" data member.
  void prepareForUserAction();
  NodeImpl *mousePressNode();

  bool isComplete();
  
  void replaceContentsWithScriptResult(const KURL &url);

    void disconnectOwnerRenderer();

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();

 private:
  int cacheId() const;

  void emitLoadEvent();
  
  void receivedFirstData();

  bool handleMouseMoveEventDrag(MouseEventWithHitTestResults*);
  bool handleMouseMoveEventOver(MouseEventWithHitTestResults*);
  void handleMouseMoveEventSelection(MouseEventWithHitTestResults*);

  /**
   * @internal Extracts anchor and tries both encoded and decoded form.
   */
  void gotoAnchor();

  void handleMousePressEventSingleClick(MouseEventWithHitTestResults*);
  void handleMousePressEventDoubleClick(MouseEventWithHitTestResults*);
  void handleMousePressEventTripleClick(MouseEventWithHitTestResults*);

  CSSComputedStyleDeclarationImpl *selectionComputedStyle(NodeImpl *&nodeToRemove) const;

    virtual void setStatusBarText(const String&);

public:
  friend class MacFrame;
  friend class FrameWin;

  void checkEmitLoadEvent();
  bool didOpenURL(const KURL &);
  virtual void didFirstLayout() {}

  virtual void frameDetached();

  virtual void detachFromView();
  void updateBaseURLForEmptyDocument();

  KURL url() const;

  // split out controller objects
  FrameTree* tree() const;
  SelectionController& selection() const;

 private:
  friend class FramePrivate;
  FramePrivate* d;
      
  mutable RefPtr<NodeImpl> _elementToDraw;
  mutable bool _drawSelectionOnly;
  KURL _submittedFormURL;
  bool m_markedTextUsesUnderlines;
  QValueList<MarkedTextUnderline> m_markedTextUnderlines;
  bool m_windowHasFocus;
  int frameCount;
};

}

#endif
