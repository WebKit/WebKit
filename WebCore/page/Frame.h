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
#include "NodeImpl.h"
#include "ObjectContents.h"
#include "edit_actions.h"
#include "text_affinity.h"
#include "text_granularity.h"
#include <qcolor.h>
#include <qscrollbar.h>

class FramePrivate;
class FrameView;
class KHTMLPartBrowserExtension;
class KHTMLSettings;

namespace KJS {
    class PausedTimeouts;
    class SavedProperties;
    class SavedBuiltins;
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
    class KJSProxyImpl;

    struct ChildFrame;
}

namespace KJS {
    class DOMDocument;
    class Selection;
    class SelectionFunc;
    class Window;
    class WindowFunc;

    namespace Bindings {
        class Instance;
    }
}

struct MarkedTextUnderline {
  MarkedTextUnderline(unsigned _startOffset, unsigned _endOffset, const QColor &_color, bool _thick) 
    : startOffset(_startOffset)
       , endOffset(_endOffset)
       , color(_color)
       , thick(_thick)
  {}
  unsigned startOffset;
  unsigned endOffset;
  QColor color;
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
  friend class WebCore::CSSStyleSelector;
  friend class WebCore::DocumentImpl;
  friend class WebCore::HTMLAnchorElementImpl;
  friend class WebCore::HTMLDocumentImpl;
  friend class WebCore::HTMLFormElementImpl;
  friend class WebCore::HTMLFrameElementImpl;
  friend class WebCore::HTMLIFrameElementImpl;
  friend class WebCore::HTMLMetaElementImpl;
  friend class WebCore::HTMLObjectElementImpl;
  friend class WebCore::HTMLTitleElementImpl;
  friend class WebCore::HTMLTokenizer;
  friend class WebCore::NodeImpl;
  friend class WebCore::RenderPartObject;
  friend class WebCore::RenderWidget;
  friend class WebCore::XMLTokenizer;

public:
  enum { NoXPosForVerticalArrowNavigation = INT_MIN };

  Frame() : d(0) { }
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
   * Returns a pointer to the @ref WebCore::BrowserExtension.
   */
  WebCore::BrowserExtension *browserExtension() const;

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
   *
   * Returns @p true if JavaScript was enabled, no error occured
   * and the code returned true itself or @p false otherwise.
   * @deprecated, use the one below.
   */
  QVariant executeScript( const QString &script, bool forceUserGesture = false );

  /**
   * Same as above except the Node parameter specifying the 'this' value.
   */
  QVariant executeScript( WebCore::NodeImpl *n, const QString &script, bool forceUserGesture = false );

  /**
   * Implementation of CSS property -khtml-user-drag == auto
   */
  virtual bool shouldDragAutoNode(WebCore::NodeImpl*, int x, int y) const;
  
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
   * a directory so that any pixmaps are found.
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
  void findTextBegin(WebCore::NodeImpl *startNode = 0, int startPos = -1);

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
   * Returns the selected part of the HTML.
   */
  WebCore::SelectionController &selection() const;

  /**
   * Returns the granularity of the selection (character, word, line, paragraph).
   */
  WebCore::ETextGranularity selectionGranularity() const;
  
  /**
   * Sets the granularity of the selection (character, word, line, paragraph).
   */
  void setSelectionGranularity(WebCore::ETextGranularity granularity) const;

  /**
   * Returns the drag caret of the HTML.
   */
  const WebCore::SelectionController &dragCaret() const;

  /**
   * Sets the current selection.
   */
  void setSelection(const WebCore::SelectionController &, bool closeTyping = true, bool keepTypingStyle = false);

  /**
   * Returns whether selection can be changed.
   */
  bool shouldChangeSelection(const WebCore::SelectionController &) const;

  /**
   * Returns a mark, to be used as emacs uses it.
   */
  const WebCore::Selection &mark() const;

  /**
   * Returns the mark.
   */
  void setMark(const WebCore::Selection &);

  /**
   * Sets the current drag caret.
   */
  void setDragCaret(const WebCore::SelectionController &);
  
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
  bool selectContentsOfNode(WebCore::NodeImpl*);
 
  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldBeginEditing(const WebCore::RangeImpl *) const;

  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldEndEditing(const WebCore::RangeImpl *) const;

  /**
   * Returns the contentEditable "override" value for the part
   */
  virtual bool isContentEditable() const;

  /**
   * Returns the most recent edit command applied.
   */
  WebCore::EditCommandPtr lastEditCommand();

  /**
   * Called when editing has been applied.
   */
  void appliedEditing(WebCore::EditCommandPtr &);

  /**
   * Called when editing has been unapplied.
   */
  void unappliedEditing(WebCore::EditCommandPtr &);

  /**
   * Called when editing has been reapplied.
   */
  void reappliedEditing(WebCore::EditCommandPtr &);

  /**
   * Returns the typing style for the document.
   */
  WebCore::CSSMutableStyleDeclarationImpl *typingStyle() const;

  /**
   * Sets the typing style for the document.
   */
  void setTypingStyle(WebCore::CSSMutableStyleDeclarationImpl *);

  /**
   * Clears the typing style for the document.
   */
  void clearTypingStyle();

  virtual void tokenizerProcessedData() {};

  const KHTMLSettings *settings() const;

  /**
   * Returns a pointer to the parent Frame, if any.
   *
   *  Returns 0L otherwise.
   */
  Frame *parentFrame() const;

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
  void computeAndSetTypingStyle(WebCore::CSSStyleDeclarationImpl *, WebCore::EditAction editingAction=WebCore::EditActionUnspecified);
  void applyStyle(WebCore::CSSStyleDeclarationImpl *, WebCore::EditAction editingAction=WebCore::EditActionUnspecified);
  void applyParagraphStyle(WebCore::CSSStyleDeclarationImpl *, WebCore::EditAction editingAction=WebCore::EditActionUnspecified);
  TriState selectionHasStyle(WebCore::CSSStyleDeclarationImpl *) const;
  bool selectionStartHasStyle(WebCore::CSSStyleDeclarationImpl *) const;
  WebCore::DOMString selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(WebCore::ElementImpl *) const;
  void removeEditingStyleFromElement(WebCore::ElementImpl *) const;
  void print();
  virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);

  // Used to keep the part alive when running a script that might destroy it.
  void keepAlive();

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

  virtual void customEvent(QCustomEvent *event);
  virtual void khtmlMouseDoubleClickEvent(WebCore::MouseDoubleClickEvent *event);
  virtual void khtmlMousePressEvent(WebCore::MousePressEvent *event);
  virtual void khtmlMouseMoveEvent(WebCore::MouseMoveEvent *event);
  virtual void khtmlMouseReleaseEvent(WebCore::MouseReleaseEvent *event);
  virtual void khtmlDrawContentsEvent(WebCore::DrawContentsEvent*) { }
  
  void selectClosestWordFromMouseEvent(QMouseEvent *mouse, WebCore::NodeImpl *innerNode, int x, int y);

  /**
   * Internal empty reimplementation of @ref ObjectContents::openFile .
   */
  virtual bool openFile();

  virtual void urlSelected( const QString &url, int button, int state,
                            const QString &_target, WebCore::URLArgs args = WebCore::URLArgs());


  // Methods with platform-specific overrides (and no base class implementation).
  virtual WebCore::BrowserExtension* createBrowserExtension() = 0;
  virtual void setTitle(const WebCore::DOMString &) = 0;
  virtual void handledOnloadEvents() = 0;
  virtual QString userAgent() const = 0;
  virtual QString incomingReferrer() const = 0;
  virtual QString mimeTypeForFileName(const QString &) const = 0;
  virtual void clearRecordedFormValues() = 0;
  virtual void recordFormValue(const QString &name, const QString &value, WebCore::HTMLFormElementImpl *element) = 0;
  virtual KJS::Bindings::Instance *getEmbedInstanceForWidget(QWidget*) = 0;
  virtual KJS::Bindings::Instance *getObjectInstanceForWidget(QWidget*) = 0;
  virtual KJS::Bindings::Instance *getAppletInstanceForWidget(QWidget*) = 0;
  virtual void markMisspellingsInAdjacentWords(const WebCore::VisiblePosition &) = 0;
  virtual void markMisspellings(const WebCore::SelectionController &) = 0;
  virtual void addMessageToConsole(const WebCore::DOMString& message,  unsigned int lineNumber, const WebCore::DOMString& sourceID) = 0;
  virtual void runJavaScriptAlert(const WebCore::DOMString& message) = 0;
  virtual bool runJavaScriptConfirm(const WebCore::DOMString& message) = 0;
  virtual bool runJavaScriptPrompt(const WebCore::DOMString& message, const WebCore::DOMString& defaultValue, WebCore::DOMString& result) = 0;  
  virtual bool locationbarVisible() = 0;
  virtual bool menubarVisible() = 0;
  virtual bool personalbarVisible() = 0;
  virtual bool statusbarVisible() = 0;
  virtual bool toolbarVisible() = 0;
  virtual void scheduleClose() = 0;
  virtual void unfocusWindow() = 0;
  virtual void createEmptyDocument() = 0;
  virtual WebCore::RangeImpl *markedTextRange() const = 0;
  virtual void registerCommandForUndo(const WebCore::EditCommandPtr &) = 0;
  virtual void registerCommandForRedo(const WebCore::EditCommandPtr &) = 0;
  virtual void clearUndoRedoOperations() = 0;
  virtual void issueUndoCommand() = 0;
  virtual void issueRedoCommand() = 0;
  virtual void issueCutCommand() = 0;
  virtual void issueCopyCommand() = 0;
  virtual void issuePasteCommand() = 0;
  virtual void issuePasteAndMatchStyleCommand() = 0;
  virtual void issueTransposeCommand() = 0;
  virtual void respondToChangedSelection(const WebCore::SelectionController &oldSelection, bool closeTyping) = 0;
  virtual void respondToChangedContents() = 0;
  virtual bool shouldChangeSelection(const WebCore::SelectionController &oldSelection, const WebCore::SelectionController &newSelection, WebCore::EAffinity affinity, bool stillSelecting) const = 0;
  virtual void partClearedInBegin() = 0; 
  virtual void saveDocumentState() = 0;
  virtual void restoreDocumentState() = 0;
  virtual bool canGoBackOrForward(int distance) const = 0;
  virtual void openURLRequest(const KURL &, const WebCore::URLArgs &) = 0;
  virtual void submitForm(const KURL &, const WebCore::URLArgs &) = 0;
  virtual void urlSelected(const KURL &url, int button, int state, const WebCore::URLArgs &args) = 0;
  virtual ObjectContents *createPart(const WebCore::ChildFrame &child, const KURL &url, const QString &mimeType) = 0;
  virtual bool passSubframeEventToSubframe(WebCore::NodeImpl::MouseEvent &) = 0;
  virtual bool passWheelEventToChildWidget(WebCore::NodeImpl *) = 0;
  virtual bool lastEventIsMouseUp() const = 0;
  virtual QString overrideMediaType() const = 0;
protected:
  virtual QString generateFrameName() = 0;

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
  void slotRedirect();
  void slotRedirection(KIO::Job*, const KURL&);
  void slotDebugDOMTree();
  void slotDebugRenderTree();

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
  void slotChildURLRequest( const KURL &url, const WebCore::URLArgs &args );
  void slotLoaderRequestDone( WebCore::DocLoader*, WebCore::CachedObject *obj );
  void checkCompleted();
  
  void slotShowDocument( const QString &url, const QString &target );
  void slotAutoScroll();
  void slotPrintFrame();
  void slotSelectAll();
  void slotProgressUpdate();
  void slotJobPercent(KIO::Job*, unsigned long);
  void slotJobSpeed(KIO::Job*, unsigned long);

  void slotEndLifeSupport();

private:
  bool restoreURL( const KURL &url );
  void clearCaretRectIfNeeded();
  void setFocusNodeIfNeeded();
  void selectionLayoutChanged();
  void timerEvent(QTimerEvent *);
  bool openURLInFrame( const KURL &url, const WebCore::URLArgs &urlArgs );

  void overURL( const QString &url, const QString &target, bool shiftPressed = false );

  bool processObjectRequest( WebCore::ChildFrame *child, const KURL &url, const QString &mimetype );
  
  void submitForm( const char *action, const QString &url, const WebCore::FormData &formData,
                   const QString &target, const QString& contentType = QString::null,
                   const QString& boundary = QString::null );

  void popupMenu( const QString &url );

  void init(FrameView *view);

  virtual void clear();

  bool scheduleScript( WebCore::NodeImpl *n, const QString& script);

  QVariant executeScheduledScript();

  bool requestFrame( WebCore::RenderPart *frame, const QString &url, const QString &frameName,
                     const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList(), bool isIFrame = false );

  /**
   * @internal returns a name for a frame without a name.
   * This function returns a sequence of names.
   * All names in a sequence are different but the sequence is
   * always the same.
   * The sequence is reset in clear().
   */
  QString requestFrameName();

  bool requestObject(WebCore::RenderPart *frame, const QString &url, const QString &serviceType,
                      const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList());
  bool requestObject(WebCore::ChildFrame *child, const KURL &url, const WebCore::URLArgs &args = WebCore::URLArgs());

public:
  WebCore::DocumentImpl *document() const;
  void setDocument(WebCore::DocumentImpl* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  virtual bool mouseDownMayStartSelect() const { return true; }

  void handleFallbackContent();

private:
  WebCore::ChildFrame* childFrame(const QObject*);
  WebCore::ChildFrame* recursiveFrameRequest(const KURL&, const WebCore::URLArgs&, bool callParent = true);

  void connectChild(const WebCore::ChildFrame *) const;
  void disconnectChild(const WebCore::ChildFrame *) const;

  bool checkLinkSecurity(const KURL &linkURL,const QString &message = QString::null, const QString &button = QString::null);
  QVariant executeScript(QString filename, int baseLine, WebCore::NodeImpl *n, const QString &script);
  
  void cancelRedirection(bool newLoadInProgress = false);

 public:
  WebCore::KJSProxyImpl *jScript();
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

  WebCore::RenderObject *renderer() const;
  IntRect selectionRect() const;
  bool isFrameSet() const;

  WebCore::HTMLFormElementImpl *currentForm() const;

  WebCore::RenderStyle *styleForSelectionStart(WebCore::NodeImpl *&nodeToRemove) const;

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
  static WebCore::NodeImpl *nodeForWidget(const QWidget *);
  static Frame *frameForNode(WebCore::NodeImpl *);

  static void setDocumentFocus(QWidget *);
  static void clearDocumentFocus(QWidget *);

  static const QPtrList<Frame> &instances() { return mutableInstances(); }    
  static QPtrList<Frame> &mutableInstances();

  void updatePolicyBaseURL();
  void setPolicyBaseURL(const WebCore::DOMString &);

  void forceLayout();
  void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

  void sendResizeEvent();
  void sendScrollEvent();
  bool scrollbarsVisible();
  void scrollToAnchor(const KURL &);
  bool canMouseDownStartSelect(WebCore::NodeImpl* node);
  bool passWidgetMouseDownEventToWidget(WebCore::MouseEvent *);
  bool passWidgetMouseDownEventToWidget(WebCore::RenderWidget *);
  virtual bool passMouseDownEventToWidget(QWidget *) = 0;

  void clearTimers();
  static void clearTimers(FrameView *);

  bool displaysWithFocusAttributes() const;
  void setWindowHasFocus(bool flag);
  // Convenience, to avoid repeating the code to dig down to get this.

  QChar backslashAsCurrencySymbol() const;
  void setName(const QString &name);
  
  QValueList<MarkedTextUnderline> markedTextUnderlines() const;  
  bool markedTextUsesUnderlines() const;
  // Call this method before handling a new user action, like on a mouse down or key down.
  // Currently, all this does is clear the "don't submit form twice" data member.
  void prepareForUserAction();
  virtual bool isFrame() const;
  WebCore::NodeImpl *mousePressNode();

  bool isComplete();
  
  void replaceContentsWithScriptResult(const KURL &url);

protected:
  mutable RefPtr<WebCore::NodeImpl> _elementToDraw;
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

  bool handleMouseMoveEventDrag(WebCore::MouseMoveEvent *event);
  bool handleMouseMoveEventOver(WebCore::MouseMoveEvent *event);
  void handleMouseMoveEventSelection(WebCore::MouseMoveEvent *event);

  /**
   * @internal Extracts anchor and tries both encoded and decoded form.
   */
  void gotoAnchor();

  void handleMousePressEventSingleClick(WebCore::MousePressEvent *event);
  void handleMousePressEventDoubleClick(WebCore::MousePressEvent *event);
  void handleMousePressEventTripleClick(WebCore::MousePressEvent *event);

  WebCore::CSSComputedStyleDeclarationImpl *selectionComputedStyle(WebCore::NodeImpl *&nodeToRemove) const;

  FramePrivate *d;
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
};

#endif
