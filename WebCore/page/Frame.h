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
#include "ObjectContents.h"
#include "edit_actions.h"
#include "text_affinity.h"
#include "text_granularity.h"
#include <kurl.h>
#include <qscrollbar.h>
#include <qstringlist.h>

class KHTMLPartBrowserExtension;
class KHTMLSettings;

namespace KJS {
    class DOMDocument;
    class JSValue;
    class PausedTimeouts;
    class SavedBuiltins;
    class SavedProperties;
    class Selection;
    class SelectionFunc;
    class Window;
    class WindowFunc;

    namespace Bindings {
        class Instance;
    }
}

namespace WebCore {

class CSSComputedStyleDeclarationImpl;
class CSSMutableStyleDeclarationImpl;
class CSSStyleDeclarationImpl;
class CSSStyleSelector;
class CachedObject;
class DOMString;
class DocLoader;
class Document;
class DocumentImpl;
class DrawContentsEvent;
class EditCommandPtr;
class ElementImpl;
class EventListener;
class FormData;
class FramePrivate;
class FrameTreeNode;
class HTMLAnchorElementImpl;
class HTMLDocument;
class HTMLDocumentImpl;
class HTMLElementImpl;
class HTMLEventListener;
class HTMLFormElementImpl;
class HTMLFrameElementImpl;
class HTMLIFrameElementImpl;
class HTMLMetaElementImpl;
class HTMLObjectElementImpl;
class HTMLTitleElementImpl;
class HTMLTokenizer;
class KJSProxyImpl;
class MacFrame;
class MouseDoubleClickEvent;
class MouseEvent;
class MouseMoveEvent;
class MousePressEvent;
class MouseReleaseEvent;
class Node;
class NodeImpl;
class Range;
class RangeImpl;
class RenderObject;
class RenderPart;
class RenderPartObject;
class RenderStyle;
class RenderWidget;
class Selection;
class SelectionController;
class VisiblePosition;
class XMLTokenizer;

struct ChildFrame;

template <typename T> class Timer;

struct MarkedTextUnderline {
    MarkedTextUnderline(unsigned s, unsigned e, const Color& c, bool t) 
        : startOffset(s), endOffset(e), color(c), thick(t) { }
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
};

class Frame : public ObjectContents {
  friend class FrameView;
  friend class KHTMLRun;
  friend class KJS::DOMDocument;
  friend class KJS::Selection;
  friend class KJS::SelectionFunc;
  friend class KJS::Window;
  friend class KJS::WindowFunc;
  friend class CSSStyleSelector;
  friend class DocumentImpl;
  friend class HTMLAnchorElementImpl;
  friend class HTMLDocumentImpl;
  friend class HTMLFormElementImpl;
  friend class HTMLFrameElementImpl;
  friend class HTMLIFrameElementImpl;
  friend class HTMLMetaElementImpl;
  friend class HTMLObjectElementImpl;
  friend class HTMLTitleElementImpl;
  friend class HTMLTokenizer;
  friend class NodeImpl;
  friend class RenderPartObject;
  friend class RenderWidget;
  friend class XMLTokenizer;

public:
  enum { NoXPosForVerticalArrowNavigation = INT_MIN };

  Frame();
  virtual ~Frame();

  /**
   * Opens the specified URL @p url.
   *
   * Reimplemented from @ref ObjectContents::openURL .
   */
  virtual bool openURL( const KURL &url );

  void didExplicitOpen();

  /**
   * Stops loading the document and kill all data requests (for images, etc.)
   */
  void stopLoading(bool sendUnload = false);
  virtual bool closeURL();

  /**
   * Returns a pointer to the @ref BrowserExtension.
   */
  BrowserExtension *browserExtension() const;

  /**
   * Returns a pointer to the HTML document's view.
   */
  FrameView *view() const;

  /**
   * Enable/disable Javascript support. Note that this will
   * in either case permanently override the default usersetting.
   * If you want to have the default UserSettings, don't call this
   * method.
   */
  void setJScriptEnabled( bool enable );

  /**
   * Returns @p true if Javascript support is enabled or @p false
   * otherwise.
   */
  bool jScriptEnabled() const;

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
   * Enables/disables Java applet support. Note that calling this function
   * will permanently override the User settings about Java applet support.
   * Not calling this function is the only way to let the default settings
   * apply.
   */
  void setJavaEnabled( bool enable );

  /**
   * Return if Java applet support is enabled/disabled.
   */
  bool javaEnabled() const;

  /**
   * Enables or disables plugins via, default is enabled
   */
  void setPluginsEnabled( bool enable );

  /**
   * Returns trie if plugins are enabled/disabled.
   */
  bool pluginsEnabled() const;

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

  /**
   * Security option.
   *
   * Specify whether only local references ( stylesheets, images, scripts, subdocuments )
   * should be loaded. ( default false - everything is loaded, if the more specific
   * options allow )
   */
  void setOnlyLocalReferences(bool enable);

  /**
   * Returns whether references should be loaded ( default false )
   **/
  bool onlyLocalReferences() const;

  void enableJScript(bool e) { setJScriptEnabled(e); }
  void enableJava(bool e) { setJavaEnabled(e); }
  void enablePlugins(bool e) { setPluginsEnabled(e); }
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

  void paint(QPainter *, const IntRect&);

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

public:

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
   * Sets the cursor to use when the cursor is on a link.
   */
  void setURLCursor( const QCursor &c );

  /**
   * Returns the cursor which is used when the cursor is on a link.
   */
  QCursor urlCursor() const;

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

  /**
   * Controls the visibility of the selection.
   */
  void setCaretVisible(bool flag=true);

  /**
   * Paints the caret.
   */
  void paintCaret(QPainter *p, const IntRect &rect) const;
  
 /**
   * Paints the drag caret.
   */
  void paintDragCaret(QPainter *p, const IntRect &rect) const;

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

  QPtrList<ObjectContents> frames() const;

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
  ObjectContents *currentFrame() const;

  /**
   * Returns whether a frame with the specified name is exists or not.
   * In contrary to the @ref findFrame method this one also returns true
   * if the frame is defined but no displaying component has been
   * found/loaded, yet.
   */
  bool frameExists( const QString &frameName );

  /**
   * Called by KJS.
   * Sets the StatusBarText assigned
   * via window.status
   */
  void setJSStatusBarText( const QString &text );

  /**
   * Called by KJS.
   * Sets the DefaultStatusBarText assigned
   * via window.defaultStatus
   */
  void setJSDefaultStatusBarText( const QString &text );

  /**
   * Called by KJS.
   * Returns the StatusBarText assigned
   * via window.status
   */
  QString jsStatusBarText() const;

  /**
   * Called by KJS.
   * Returns the DefaultStatusBarText assigned
   * via window.defaultStatus
   */
  QString jsDefaultStatusBarText() const;

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
  bool canPaste() const;
  void redo();
  void undo();
  bool canRedo() const;
  bool canUndo() const;
  void computeAndSetTypingStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  void applyStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  void applyParagraphStyle(CSSStyleDeclarationImpl *, EditAction editingAction=EditActionUnspecified);
  TriState selectionHasStyle(CSSStyleDeclarationImpl *) const;
  bool selectionStartHasStyle(CSSStyleDeclarationImpl *) const;
  DOMString selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(ElementImpl *) const;
  void removeEditingStyleFromElement(ElementImpl *) const;
  void print();
  virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);

  // Used to keep the part alive when running a script that might destroy it.
  void keepAlive();

  static void endAllLifeSupport();

signals:
  /**
   * Emitted if the cursor is moved over an URL.
   */
  void onURL( const QString &url );

  /**
   * Emitted when the user clicks the right mouse button on the document.
   */
  void popupMenu(const QString &url, const IntPoint &point);

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

  virtual void khtmlMouseDoubleClickEvent(MouseDoubleClickEvent *event);
  virtual void khtmlMousePressEvent(MousePressEvent *event);
  virtual void khtmlMouseMoveEvent(MouseMoveEvent *event);
  virtual void khtmlMouseReleaseEvent(MouseReleaseEvent *event);
  virtual void khtmlDrawContentsEvent(DrawContentsEvent*) { }
  
  void selectClosestWordFromMouseEvent(QMouseEvent *mouse, NodeImpl *innerNode, int x, int y);

  /**
   * Internal empty reimplementation of @ref ObjectContents::openFile .
   */
  virtual bool openFile();

  virtual void urlSelected( const QString &url, int button, int state,
                            const QString &_target, URLArgs args = URLArgs());


  // Methods with platform-specific overrides (and no base class implementation).
  virtual BrowserExtension* createBrowserExtension() = 0;
  virtual void setTitle(const DOMString &) = 0;
  virtual void handledOnloadEvents() = 0;
  virtual QString userAgent() const = 0;
  virtual QString incomingReferrer() const = 0;
  virtual QString mimeTypeForFileName(const QString &) const = 0;
  virtual void clearRecordedFormValues() = 0;
  virtual void recordFormValue(const QString &name, const QString &value, HTMLFormElementImpl *element) = 0;
  virtual KJS::Bindings::Instance *getEmbedInstanceForWidget(QWidget*) = 0;
  virtual KJS::Bindings::Instance *getObjectInstanceForWidget(QWidget*) = 0;
  virtual KJS::Bindings::Instance *getAppletInstanceForWidget(QWidget*) = 0;
  virtual void markMisspellingsInAdjacentWords(const VisiblePosition &) = 0;
  virtual void markMisspellings(const SelectionController &) = 0;
  virtual void addMessageToConsole(const DOMString& message,  unsigned int lineNumber, const DOMString& sourceID) = 0;
  virtual void runJavaScriptAlert(const DOMString& message) = 0;
  virtual bool runJavaScriptConfirm(const DOMString& message) = 0;
  virtual bool runJavaScriptPrompt(const DOMString& message, const DOMString& defaultValue, DOMString& result) = 0;  
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
  virtual void urlSelected(const KURL &url, int button, int state, const URLArgs &args) = 0;
  virtual ObjectContents *createPart(const ChildFrame &child, const KURL &url, const QString &mimeType) = 0;
  virtual bool passSubframeEventToSubframe(NodeImpl::MouseEvent &) = 0;
  virtual bool passWheelEventToChildWidget(NodeImpl *) = 0;
  virtual bool lastEventIsMouseUp() const = 0;
  virtual QString overrideMediaType() const = 0;
protected:
  virtual DOMString generateFrameName() = 0;

public slots:
  /**
   * Stops all animated images on the current and child pages
   */
  void stopAnimations();

private slots:
  void reparseConfiguration();

  void slotData( KIO::Job*, const ByteArray &data );
  void slotRestoreData( const ByteArray &data );
  void slotFinished( KIO::Job* );
  void slotFinishedParsing();
    void redirectionTimerFired(Timer<Frame>*);
  void slotRedirection(KIO::Job*, const KURL&);

  void slotIncZoom();
  void slotDecZoom();

  void childBegin();

  void slotLoadImages();

  void submitFormAgain();

  void updateActions();

  void slotPartRemoved( ObjectContents *part );

  void slotActiveFrameChanged( ObjectContents *part );

  void slotChildStarted( KIO::Job *job );

  void slotChildCompleted();
  void slotChildCompleted( bool );
  void slotParentCompleted();
  void slotChildURLRequest( const KURL &url, const URLArgs &args );
  void slotLoaderRequestDone( DocLoader*, CachedObject *obj );
  void checkCompleted();
  
  void slotShowDocument( const QString &url, const QString &target );
  void slotAutoScroll();
  void slotPrintFrame();
  void slotSelectAll();
  void slotProgressUpdate();
  void slotJobPercent(KIO::Job*, unsigned long);
  void slotJobSpeed(KIO::Job*, unsigned long);

    void lifeSupportTimerFired(Timer<Frame>*);
    void endLifeSupport();

  virtual void clear(bool clearWindowProperties = true);

private:
  bool restoreURL( const KURL &url );
  void clearCaretRectIfNeeded();
  void setFocusNodeIfNeeded();
  void selectionLayoutChanged();
    void caretBlinkTimerFired(Timer<Frame>*);
  bool openURLInFrame( const KURL &url, const URLArgs &urlArgs );

  void overURL( const QString &url, const QString &target, bool shiftPressed = false );

  bool processObjectRequest( ChildFrame *child, const KURL &url, const QString &mimetype );
  
  void submitForm( const char *action, const QString &url, const FormData &formData,
                   const QString &target, const QString& contentType = QString::null,
                   const QString& boundary = QString::null );

  void popupMenu( const QString &url );

  void init(FrameView*);

  bool scheduleScript(NodeImpl*, const QString& script);

  KJS::JSValue* executeScheduledScript();

    bool requestFrame(RenderPart *frame, const QString &url, const QString &frameName,
        const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList(), bool isIFrame = false);

  /**
   * @internal returns a name for a frame without a name.
   * This function returns a sequence of names.
   * All names in a sequence are different but the sequence is
   * always the same.
   * The sequence is reset in clear().
   */

  bool requestObject(RenderPart *frame, const QString &url, const QString &serviceType,
                      const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList());
  bool requestObject(ChildFrame *child, const KURL &url, const URLArgs &args = URLArgs());

public:
  DOMString requestFrameName();

  DocumentImpl *document() const;
  void setDocument(DocumentImpl* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  virtual bool mouseDownMayStartSelect() const { return true; }

  void handleFallbackContent();

private:
  ChildFrame* childFrame(const QObject*);
  ChildFrame* recursiveFrameRequest(const KURL&, const URLArgs&, bool callParent = true);

  void connectChild(const ChildFrame *) const;
  void disconnectChild(const ChildFrame *) const;

  bool checkLinkSecurity(const KURL &linkURL,const QString &message = QString::null, const QString &button = QString::null);
  KJS::JSValue* executeScript(const QString& filename, int baseLine, NodeImpl*, const QString& script);
  
  void cancelRedirection(bool newLoadInProgress = false);

 public:
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

  RenderObject *renderer() const;
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

  static Frame *frameForWidget(const QWidget *);
  static NodeImpl *nodeForWidget(const QWidget *);
  static Frame *frameForNode(NodeImpl *);

  static void setDocumentFocus(QWidget *);
  static void clearDocumentFocus(QWidget *);

  static const QPtrList<Frame> &instances() { return mutableInstances(); }    
  static QPtrList<Frame> &mutableInstances();

  void updatePolicyBaseURL();
  void setPolicyBaseURL(const DOMString &);

  void forceLayout();
  void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

  void sendResizeEvent();
  void sendScrollEvent();
  bool scrollbarsVisible();
  void scrollToAnchor(const KURL &);
  bool canMouseDownStartSelect(NodeImpl* node);
  bool passWidgetMouseDownEventToWidget(MouseEvent *, bool isDoubleClick);
  bool passWidgetMouseDownEventToWidget(RenderWidget *);
  virtual bool passMouseDownEventToWidget(QWidget *) = 0;

  void clearTimers();
  static void clearTimers(FrameView *);

  bool displaysWithFocusAttributes() const;
  void setWindowHasFocus(bool flag);
  // Convenience, to avoid repeating the code to dig down to get this.

  QChar backslashAsCurrencySymbol() const;

  QValueList<MarkedTextUnderline> markedTextUnderlines() const;  
  bool markedTextUsesUnderlines() const;
  // Call this method before handling a new user action, like on a mouse down or key down.
  // Currently, all this does is clear the "don't submit form twice" data member.
  void prepareForUserAction();
  virtual bool isFrame() const;
  NodeImpl *mousePressNode();

  bool isComplete();
  
  void replaceContentsWithScriptResult(const KURL &url);

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();

  mutable RefPtr<NodeImpl> _elementToDraw;
  mutable bool _drawSelectionOnly;
  KURL _submittedFormURL;
  bool m_markedTextUsesUnderlines;
  QValueList<MarkedTextUnderline> m_markedTextUnderlines;
  bool m_windowHasFocus;

 private:
  int cacheId() const;

  void checkEmitLoadEvent();
  void emitLoadEvent();
  
  void receivedFirstData();

  bool handleMouseMoveEventDrag(MouseMoveEvent *event);
  bool handleMouseMoveEventOver(MouseMoveEvent *event);
  void handleMouseMoveEventSelection(MouseMoveEvent *event);

  /**
   * @internal Extracts anchor and tries both encoded and decoded form.
   */
  void gotoAnchor();

  void handleMousePressEventSingleClick(MousePressEvent *event);
  void handleMousePressEventDoubleClick(MousePressEvent *event);
  void handleMousePressEventTripleClick(MousePressEvent *event);

  CSSComputedStyleDeclarationImpl *selectionComputedStyle(NodeImpl *&nodeToRemove) const;

  FramePrivate* d;
  friend class FramePrivate;

public:
  friend class MacFrame;

  void completed();
  void completed(bool);
  bool didOpenURL(const KURL &);
  void setStatusBarText(const QString &);
  void started(KIO::Job *);
  void frameDetached();
  virtual void didFirstLayout() {}

  int frameCount;

  virtual void detachFromView();

  KURL url() const;

  // split out controller objects
  FrameTreeNode* treeNode();
  SelectionController& selection() const;
};

}

#endif
