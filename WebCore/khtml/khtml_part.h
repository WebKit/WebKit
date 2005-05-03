// -*- c-basic-offset: 2 -*-
/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef __khtml_part_h__
#define __khtml_part_h__

#include "editing/text_granularity.h"
#include "editing/edit_actions.h"

#include <kparts/part.h>
#include <kparts/browserextension.h>

class KHTMLPartPrivate;
class KHTMLPartBrowserExtension;
class KJSProxy;
class KHTMLView;
class KHTMLSettings;
class KJavaAppletContext;

namespace DOM
{
  class CSSComputedStyleDeclarationImpl;
  class CSSMutableStyleDeclarationImpl;
  class CSSStyleDeclarationImpl;
  class DOMString;
  class Document;
  class DocumentImpl;
  class ElementImpl;
  class EventListener;
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
  class Node;
  class NodeImpl;
  class Range;
}

namespace khtml
{
  class CachedObject;
  struct ChildFrame;
  class CSSStyleSelector;
  class DocLoader;
  class DrawContentsEvent;
  class EditCommandPtr;
  class FormData;
  class HTMLTokenizer;
  class MouseDoubleClickEvent;
  class MouseEvent;
  class MouseMoveEvent;
  class MousePressEvent;
  class MouseReleaseEvent;
  class RenderPart;
  class RenderPartObject;
  class RenderWidget;
  class Selection;
  class XMLTokenizer;
}

namespace KJS {
    class DOMDocument;
    class Selection;
    class SelectionFunc;
    class Window;
    class WindowFunc;
}

namespace KParts
{
  class PartManager;
};

/**
 * This class is khtml's main class. It features an almost complete
 * web browser, and html renderer.
 *
 * The easiest way to use this class (if you just want to display an HTML
 * page at some URL) is the following:
 *
 * <pre>
 * KURL url = "http://www.kde.org";
 * KHTMLPart *w = new KHTMLPart();
 * w->openURL(url);
 * w->view()->resize(500, 400);
 * w->show();
 * </pre>
 *
 * By default Java and JavaScript support are disabled. You can enable them by
 * using the @ref setJavaEnabled() and @ref setJScriptEnabled() methods.
 *
 * Some apps want to write their HTML code directly into the widget instead of
 * it opening an url. You can also do that in the following way:
 *
 * <pre>
 * QString myHTMLCode = ...;
 * KHTMLPart *w = new KHTMLPart();
 * w->begin();
 * w->write(myHTMLCode);
 * ...
 * w->end();
 * </pre>
 *
 * You can do as many calls to write as you want. But there are two
 * @ref write() methods, one accepting a @ref QString one accepting a
 * @p char @p * argument. You should use one or the other
 * (but not both) since the method using
 * the @p char @p * argument does an additional decoding step to convert the
 * written data to Unicode.
 *
 * @short HTML Browser Widget
 * @author Lars Knoll (knoll@kde.org)
 * */
class KHTMLPart : public KParts::ReadOnlyPart
{
  Q_OBJECT
  friend class KHTMLView;
  friend class DOM::HTMLTitleElementImpl;
  friend class DOM::HTMLFrameElementImpl;
  friend class DOM::HTMLIFrameElementImpl;
  friend class DOM::HTMLObjectElementImpl;
  friend class DOM::HTMLAnchorElementImpl;
  friend class DOM::HTMLMetaElementImpl;
  friend class DOM::NodeImpl;
  friend class KHTMLRun;
  friend class DOM::HTMLFormElementImpl;
  friend class khtml::RenderPartObject;
  friend class KJS::Selection;
  friend class KJS::SelectionFunc;
  friend class KJS::Window;
  friend class KJS::WindowFunc;
  friend class KJS::DOMDocument;
  friend class KJSProxy;
  friend class KHTMLPartBrowserExtension;
  friend class DOM::DocumentImpl;
  friend class DOM::HTMLDocumentImpl;
  friend class KHTMLPartBrowserHostExtension;
  friend class khtml::HTMLTokenizer;
  friend class khtml::XMLTokenizer;
  friend class khtml::RenderWidget;
  friend class khtml::CSSStyleSelector;
  friend class KHTMLPartIface;

  Q_PROPERTY( bool javaScriptEnabled READ jScriptEnabled WRITE setJScriptEnabled )
  Q_PROPERTY( bool javaEnabled READ javaEnabled WRITE setJavaEnabled )
  Q_PROPERTY( bool autoloadImages READ autoloadImages WRITE setAutoloadImages )
  Q_PROPERTY( bool dndEnabled READ dndEnabled WRITE setDNDEnabled )
  Q_PROPERTY( bool pluginsEnabled READ pluginsEnabled WRITE setPluginsEnabled )
  Q_PROPERTY( bool onlyLocalReferences READ onlyLocalReferences WRITE setOnlyLocalReferences )
  Q_PROPERTY( QCString dcopObjectId READ dcopObjectId )

public:
  enum GUIProfile { DefaultGUI, BrowserViewGUI /* ... */ };
  enum { NoXPosForVerticalArrowNavigation = INT_MIN };

  /**
   * Constructs a new KHTMLPart.
   *
   * KHTML basically consists of two objects: The KHTMLPart itself,
   * holding the document data (DOM document), and the @ref KHTMLView,
   * derived from @ref QScrollView, in which the document content is
   * rendered in. You can specify two different parent objects for a
   * KHTMLPart, one parent for the KHTMLPart document and on parent
   * for the @ref KHTMLView. If the second @p parent argument is 0L, then
   * @p parentWidget is used as parent for both objects, the part and
   * the view.
   */
  KHTMLPart( QWidget *parentWidget = 0, const char *widgetname = 0,
             QObject *parent = 0, const char *name = 0, GUIProfile prof = DefaultGUI );

  KHTMLPart( KHTMLView *view, QObject *parent = 0, const char *name = 0, GUIProfile prof = DefaultGUI );

  /**
   * Destructor.
   */
  virtual ~KHTMLPart();

  /**
   * Opens the specified URL @p url.
   *
   * Reimplemented from @ref KParts::ReadOnlyPart::openURL .
   */
  virtual bool openURL( const KURL &url );

  void didExplicitOpen();

  /**
   * Stops loading the document and kill all data requests (for images, etc.)
   */
  virtual bool closeURL();

#if !APPLE_CHANGES
  /**
   * is called when a certain error situation (i.e. connection timed out) occured.
   * default implementation either shows a KIO error dialog or loads a more verbose
   * error description a as page, depending on the users configuration.
   * @p job is the job that signaled the error situation
   */
  virtual void showError(KIO::Job* job);
#endif

  /**
   * Returns a reference to the DOM HTML document (for non-HTML documents, returns null)
   */
  DOM::HTMLDocument htmlDocument() const;

  /**
   * Returns a reference to the DOM document.
   */
  DOM::Document document() const;

  /**
   * Returns the node that has the keyboard focus.
   */
  DOM::Node activeNode() const;

  /**
   * Returns a pointer to the @ref KParts::BrowserExtension.
   */
  KParts::BrowserExtension *browserExtension() const;

  /**
   * Returns a pointer to the HTML document's view.
   */
  KHTMLView *view() const;

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
  QVariant executeScript( const DOM::Node &n, const QString &script, bool forceUserGesture = false );

  /**
   * Enables or disables Drag'n'Drop support. A drag operation is started if
   * the users drags a link.
   */
  void setDNDEnabled( bool b );
  /**
   * Returns whether Dragn'n'Drop support is enabled or not.
   */
  bool dndEnabled() const;

  /**
   * Implementation of CSS property -khtml-user-drag == auto
   */
  virtual bool shouldDragAutoNode(DOM::NodeImpl*, int x, int y) const;
  
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
   * Returns the java context of the applets. If no applet exists, 0 is returned.
   */
  KJavaAppletContext *javaContext();

  /**
   * Returns the java context of the applets. If no context exists yet, a new one is
   * created.
   */
  KJavaAppletContext *createJavaContext();

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
   * @note that the returned information is unrelieable as long as no begin()
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

#ifndef KDE_NO_COMPAT
  void enableJScript(bool e) { setJScriptEnabled(e); }
  void enableJava(bool e) { setJavaEnabled(e); }
  void enablePlugins(bool e) { setPluginsEnabled(e); }
  void autoloadImages(bool e) { setAutoloadImages(e); }
  void enableMetaRefresh(bool e) { setMetaRefreshEnabled(e); }
  bool setCharset( const QString &, bool ) { return true; }

  KURL baseURL() const;
  QString baseTarget() const;
#endif

  /**
   * Returns the URL for the background Image (used by save background)
   */
  KURL backgroundURL() const;

  /**
   * Schedules a redirection after @p delay seconds.
   */
  void scheduleRedirection(double delay, const QString &url, bool lockHistory = true);

  /**
   * Schedules a location change.
   * This is used for JavaScript-triggered location changes.
   */
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

  /*
   * Prints the current HTML page layed out for the printer.
   *
   * (not implemented at the moment)
   */
  //    void print(QPainter *, int pageHeight, int pageWidth);

  /**
   * Paints the HTML page to a QPainter. See @ref KHTMLView::paint for details
   */
  void paint(QPainter *, const QRect &, int = 0, bool * = 0);

  /**
   * Sets the encoding the page uses.
   *
   * This can be different from the charset. The widget will try to reload the current page in the new
   * encoding, if url() is not empty.
   */
  bool setEncoding( const QString &name, bool override = false );

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
  void findTextBegin(DOM::NodeImpl *startNode = 0, int startPos = -1);

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
  const khtml::Selection &selection() const;

  /**
   * Returns the granularity of the selection (character, word, line, paragraph).
   */
  khtml::ETextGranularity selectionGranularity() const;
  
  /**
   * Sets the granularity of the selection (character, word, line, paragraph).
   */
  void KHTMLPart::setSelectionGranularity(khtml::ETextGranularity granularity) const;

  /**
   * Returns the drag caret of the HTML.
   */
  const khtml::Selection &dragCaret() const;

  /**
   * Sets the current selection.
   */
  void setSelection(const khtml::Selection &, bool closeTyping = true, bool keepTypingStyle = false);

  /**
   * Returns a mark, to be used as emacs uses it.
   */
  const khtml::Selection &mark() const;

  /**
   * Returns the mark.
   */
  void setMark(const khtml::Selection &);

  /**
   * Sets the current drag cart.
   */
  void setDragCaret(const khtml::Selection &);
  
  /**
   * Clears the current selection.
   */
  void clearSelection();

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
  void paintCaret(QPainter *p, const QRect &rect) const;
  
 /**
   * Paints the drag caret.
   */
  void paintDragCaret(QPainter *p, const QRect &rect) const;

  /**
   * Set info for vertical arrow navigation.
   */
  void setXPosForVerticalArrowNavigation(int x);

  /**
   * Get info for vertical arrow navigation.
   */
  int xPosForVerticalArrowNavigation() const;

  /**
   * Returns the text for a part of the document.
   */
  QString text(const DOM::Range &) const;

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
   * Returns whether editing should end in the given range
   */
  virtual bool shouldBeginEditing(const DOM::Range &) const;

  /**
   * Returns whether editing should end in the given range
   */
  virtual bool shouldEndEditing(const DOM::Range &) const;

  /**
   * Returns the contentEditable "override" value for the part
   */
  virtual bool isContentEditable() const;

  /**
   * Returns the most recent edit command applied.
   */
  khtml::EditCommandPtr lastEditCommand();

  /**
   * Called when editing has been applied.
   */
  void appliedEditing(khtml::EditCommandPtr &);

  /**
   * Called when editing has been unapplied.
   */
  void unappliedEditing(khtml::EditCommandPtr &);

  /**
   * Called when editing has been reapplied.
   */
  void reappliedEditing(khtml::EditCommandPtr &);

  /**
   * Returns the typing style for the document.
   */
  DOM::CSSMutableStyleDeclarationImpl *typingStyle() const;

  /**
   * Sets the typing style for the document.
   */
  void setTypingStyle(DOM::CSSMutableStyleDeclarationImpl *);

  /**
   * Clears the typing style for the document.
   */
  void clearTypingStyle();

  /**
   * Convenience method to show the document's view.
   *
   * Equivalent to widget()->show() or view()->show() .
   */
  void show();

  /**
   * Convenience method to hide the document's view.
   *
   * Equivalent to widget()->hide() or view()->hide().
   */
  void hide();

  /**
   * Returns a reference to the partmanager instance which
   * manages html frame objects.
   */
  KParts::PartManager *partManager();

#if !APPLE_CHANGES
  /**
   * Saves the KHTMLPart's complete state (including child frame
   * objects) to the provided @ref QDataStream.
   *
   * This is called from the @ref saveState() method of the
   *  @ref browserExtension().
   */
  virtual void saveState( QDataStream &stream );
  /**
   * Restores the KHTMLPart's previously saved state (including
   * child frame objects) from the provided QDataStream.
   *
   * @see saveState()
   *
   * This is called from the @ref restoreState() method of the
   * @ref browserExtension() .
   **/
  virtual void restoreState( QDataStream &stream );
#endif

  virtual void tokenizerProcessedData() {};

  /**
   * Returns the @p Node currently under the mouse
   */
  DOM::Node nodeUnderMouse() const;

  /**
   * @internal
   */
  const KHTMLSettings *settings() const;

  /**
   * Returns a pointer to the parent KHTMLPart if the part is a frame
   * in an HTML frameset.
   *
   *  Returns 0L otherwise.
   */
  KHTMLPart *parentPart() const;

  /**
   * Returns a list of names of all frame (including iframe) objects of
   * the current document. Note that this method is not working recursively
   * for sub-frames.
   */
  QStringList frameNames() const;

  QPtrList<KParts::ReadOnlyPart> frames() const;

  KHTMLPart *childFrameNamed(const QString &name) const;

  /**
   * Finds a frame by name. Returns 0L if frame can't be found.
   */
  KHTMLPart *findFrame( const QString &f );

  /**
   * Return the current frame (the one that has focus)
   * Not necessarily a direct child of ours, framesets can be nested.
   * Returns "this" if this part isn't a frameset.
   */
  KParts::ReadOnlyPart *currentFrame() const;

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
   * Loads a style sheet into the stylesheet cache.
   */
  void preloadStyleSheet(const QString &url, const QString &stylesheet);

  /**
   * Loads a script into the script cache.
   */
  void preloadScript(const QString &url, const QString &script);

  bool isPointInsideSelection(int x, int y);

  virtual bool tabsToLinks() const;
  virtual bool tabsToAllControls() const;
  
  /**
   * @internal
   */
  bool restored() const;

  void incrementFrameCount();
  void decrementFrameCount();
  int topLevelFrameCount();

  // Editing operations.
  // Not clear if these will be wanted in KHTMLPart by KDE,
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
  void computeAndSetTypingStyle(DOM::CSSStyleDeclarationImpl *, khtml::EditAction editingAction=khtml::EditActionUnspecified);
  void applyStyle(DOM::CSSStyleDeclarationImpl *, khtml::EditAction editingAction=khtml::EditActionUnspecified);
  void applyParagraphStyle(DOM::CSSStyleDeclarationImpl *, khtml::EditAction editingAction=khtml::EditActionUnspecified);
  TriState selectionHasStyle(DOM::CSSStyleDeclarationImpl *) const;
  bool selectionStartHasStyle(DOM::CSSStyleDeclarationImpl *) const;
  DOM::DOMString selectionStartStylePropertyValue(int stylePropertyID) const;
  void applyEditingStyleToBodyElement() const;
  void removeEditingStyleFromBodyElement() const;
  void applyEditingStyleToElement(DOM::ElementImpl *) const;
  void removeEditingStyleFromElement(DOM::ElementImpl *) const;
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
  void popupMenu(const QString &url, const QPoint &point);

  /**
   * This signal is emitted when the selection changes.
   */
  void selectionChanged();

  /**
   * This signal is emitted when an element retrieves the
   * keyboard focus. Note that the signal argument can be
   * a null node if no element is active, meaning a node
   * has explicitly been deactivated without a new one
   * becoming active.
   */
  void nodeActivated(const DOM::Node &);

public:

  /**
   * returns a KURL object for the given url. Use when
   * you know what you're doing.
   */
  KURL completeURL( const QString &url );

protected:

  /**
   * presents a detailed error message to the user.
   * @p errorCode kio error code, eg KIO::ERR_SERVER_TIMEOUT.
   * @p text kio additional information text.
   * @p url the url that triggered the error.
   */
  void htmlError(int errorCode, const QString& text, const KURL& reqUrl);

  virtual void customEvent( QCustomEvent *event );

  /**
   * Eventhandler of the khtml::MousePressEvent.
   */
  virtual void khtmlMousePressEvent( khtml::MousePressEvent *event );
  /**
   * Eventhandler for the khtml::MouseDoubleClickEvent.
   */
  virtual void khtmlMouseDoubleClickEvent( khtml::MouseDoubleClickEvent * );

  /**
   * Eventhandler for the khtml::MouseDoubleMoveEvent.
   */
  virtual void khtmlMouseMoveEvent( khtml::MouseMoveEvent *event );
  /**
   * Eventhandler for the khtml::MouseMoveEvent.
   */
  virtual void khtmlMouseReleaseEvent( khtml::MouseReleaseEvent *event );
  /**
   * Eventhandler for the khtml::DrawContentsEvent.
   */
  virtual void khtmlDrawContentsEvent( khtml::DrawContentsEvent * );
  
  void selectClosestWordFromMouseEvent(QMouseEvent *mouse, DOM::Node &innerNode, int x, int y);

#if !APPLE_CHANGES
  /**
   * Internal reimplementation of KParts::Part::guiActivateEvent .
   */
  virtual void guiActivateEvent( KParts::GUIActivateEvent *event );
#endif

  /**
   * Internal empty reimplementation of @ref KParts::ReadOnlyPart::openFile .
   */
  virtual bool openFile();

  virtual void urlSelected( const QString &url, int button, int state,
                            const QString &_target, KParts::URLArgs args = KParts::URLArgs());

#if !APPLE_CHANGES
  /**
   * This method is called when a new embedded object (include html frames) is to be created.
   * Reimplement it if you want to add support for certain embeddable objects without registering
   * them in the KDE wide registry system (KSyCoCa) . Another reason for re-implementing this
   * method could be if you want to derive from KTHMLPart and also want all html frame objects
   * to be a object of your derived type, in which case you should return a new instance for
   * the mimetype 'text/html' .
   */
  virtual KParts::ReadOnlyPart *createPart( QWidget *parentWidget, const char *widgetName,
                                            QObject *parent, const char *name,
                                            const QString &mimetype, QString &serviceName,
                                            QStringList &serviceTypes, const QStringList &params);
#endif

public slots:

  /**
   * Sets the focussed node of the document to the specified node. If the node is a form control, the control will
   * receive focus in the same way that it would if the user had clicked on it or tabbed to it with the keyboard. For
   * most other types of elements, there is no visul indiction of whether or not they are focussed.
   *
   * See @ref activeNode
   *
   * @param node The node to focus
   */
  void setActiveNode(const DOM::Node &node);

  /**
   * Stops all animated images on the current and child pages
   */
  void stopAnimations();

  QCString dcopObjectId() const;

private slots:

  /**
   * @internal
   */
  void reparseConfiguration();

  /**
   * @internal
   */
  void slotData( KIO::Job*, const QByteArray &data );
  /**
   * @internal
   */
  void slotRestoreData( const QByteArray &data );
  /**
   * @internal
   */
  void slotFinished( KIO::Job* );
  /**
   * @internal
   */
  void slotFinishedParsing();
  /**
   * @internal
   */
  void slotRedirect();
  /**
   * @internal
   */
  void slotRedirection(KIO::Job*, const KURL&);
  /**
   * @internal
   */
  void slotDebugDOMTree();
  /**
   * @internal
   */
  void slotDebugRenderTree();
#if !APPLE_CHANGES
  /**
   * @internal
   */
  virtual void slotViewDocumentSource();
  /**
   * @internal
   */
  virtual void slotViewFrameSource();
  /**
   * @internal
   */
  virtual void slotSaveBackground();
  /**
   * @internal
   */
  virtual void slotSaveDocument();
  /**
   * @internal
   */
  virtual void slotSaveFrame();
  /**
   * @internal
   */
  virtual void slotSecurity();
  /**
   * @internal
   */
  virtual void slotSetEncoding();

  /**
   * @internal
   */
  virtual void slotUseStylesheet();

  virtual void slotFind();
  virtual void slotFindDone();
  virtual void slotFindDialogDestroyed();
#endif

  void slotIncZoom();
  void slotDecZoom();

#if APPLE_CHANGES
  void childBegin();
#endif

  void slotLoadImages();

  /**
   * @internal
   */
  void submitFormAgain();

  /**
   * @internal
   */
  void updateActions();
  /**
   * @internal
   */
  void slotPartRemoved( KParts::Part *part );
  /**
   * @internal
   */
  void slotActiveFrameChanged( KParts::Part *part );
  /**
   * @internal
   */
  void slotChildStarted( KIO::Job *job );
  /**
   * @internal
   */
  void slotChildCompleted();
  /**
   * @internal
   */
  void slotChildCompleted( bool );
  /**
   * @internal
   */
  void slotParentCompleted();
  /**
   * @internal
   */
  void slotChildURLRequest( const KURL &url, const KParts::URLArgs &args );

  void slotLoaderRequestStarted( khtml::DocLoader*, khtml::CachedObject* obj);
  void slotLoaderRequestDone( khtml::DocLoader*, khtml::CachedObject *obj );
  void checkCompleted();

  /**
   * @internal
   */
  void slotShowDocument( const QString &url, const QString &target );

  /**
   * @internal
   */
  void slotAutoScroll();

  void slotPrintFrame();

  void slotSelectAll();

  /**
   * @internal
   */
  void slotProgressUpdate();

  /*
   * @internal
   */
  void slotJobPercent(KIO::Job*, unsigned long);

  /*
   * @internal
   */
  void slotJobSpeed(KIO::Job*, unsigned long);

  /**
   * @internal
   */
  void slotClearSelection();

  void slotEndLifeSupport();

private:


  /**
   * @internal
   */
  bool restoreURL( const KURL &url );

  /**
   * @internal
   */
  void clearCaretRectIfNeeded();

  /**
   * @internal
   */
  void setFocusNodeIfNeeded();

  /**
   * @internal
   */
  void selectionLayoutChanged();

  /**
   * @internal
   */
  void emitSelectionChanged();

  /**
   * @internal
   */
  void timerEvent(QTimerEvent *);
  
  /**
   * @internal
   */
  bool openURLInFrame( const KURL &url, const KParts::URLArgs &urlArgs );

  void startAutoScroll();
  void stopAutoScroll();
  void overURL( const QString &url, const QString &target, bool shiftPressed = false );

  /**
   * @internal
   */
  bool processObjectRequest( khtml::ChildFrame *child, const KURL &url, const QString &mimetype );

  /**
   * @internal
   */
  void submitForm( const char *action, const QString &url, const khtml::FormData &formData,
                   const QString &target, const QString& contentType = QString::null,
                   const QString& boundary = QString::null );

  /**
   * @internal
   */
  void popupMenu( const QString &url );


  void init( KHTMLView *view, GUIProfile prof );

  virtual void clear();

  bool scheduleScript( const DOM::Node &n, const QString& script);

  QVariant executeScheduledScript();

  bool requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                     const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList(), bool isIFrame = false );

  /**
   * @internal returns a name for a frame without a name.
   * This function returns a sequence of names.
   * All names in a sequence are different but the sequence is
   * always the same.
   * The sequence is reset in clear().
   */
  QString requestFrameName();

  bool requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                      const QStringList &paramNames = QStringList(), const QStringList &paramValues = QStringList()  );

  bool requestObject( khtml::ChildFrame *child, const KURL &url, const KParts::URLArgs &args = KParts::URLArgs() );

  DOM::EventListener *createHTMLEventListener( QString code, DOM::NodeImpl *node );

public:
  DOM::DocumentImpl *xmlDocImpl() const;
  void replaceDocImpl(DOM::DocumentImpl* newDoc);

  // Workaround for the fact that it's hard to delete a frame.
  // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
  void selectFrameElementInParentIfFullySelected();
  
  void handleFallbackContent();

private:
  khtml::ChildFrame *childFrame( const QObject *obj );

  khtml::ChildFrame *recursiveFrameRequest( const KURL &url, const KParts::URLArgs &args, bool callParent = true );

  void connectChild(const khtml::ChildFrame *) const;
  void disconnectChild(const khtml::ChildFrame *) const;

  bool checkLinkSecurity(const KURL &linkURL,const QString &message = QString::null, const QString &button = QString::null);
  QVariant executeScript(QString filename, int baseLine, const DOM::Node &n, const QString &script);
  
  void cancelRedirection(bool newLoadInProgress = false);

  KJSProxy *jScript();

#if APPLE_CHANGES
 public:
#endif
  KHTMLPart *opener();
#if APPLE_CHANGES
 private:
#endif
  long cacheId() const;
  void setOpener(KHTMLPart *_opener);
  bool openedByJS();
  void setOpenedByJS(bool _openedByJS);

  void checkEmitLoadEvent();
  void emitLoadEvent();
  
  void receivedFirstData();

  void replaceContentsWithScriptResult( const KURL &url );

  bool handleMouseMoveEventDrag(khtml::MouseMoveEvent *event);
  bool handleMouseMoveEventOver(khtml::MouseMoveEvent *event);
  void handleMouseMoveEventSelection(khtml::MouseMoveEvent *event);

  /**
   * @internal Extracts anchor and tries both encoded and decoded form.
   */
  void gotoAnchor();

#if APPLE_CHANGES
  void handleMousePressEventSingleClick(khtml::MousePressEvent *event);
  void handleMousePressEventDoubleClick(khtml::MousePressEvent *event);
  void handleMousePressEventTripleClick(khtml::MousePressEvent *event);
#endif

  DOM::CSSComputedStyleDeclarationImpl *selectionComputedStyle(DOM::NodeImpl *&nodeToRemove) const;

  KHTMLPartPrivate *d;
  friend class KHTMLPartPrivate;
  friend class khtml::Selection;

#if APPLE_CHANGES
public:  
  friend class KWQKHTMLPart;

  void completed();
  void completed(bool);
  bool didOpenURL(const KURL &);
  void setStatusBarText(const QString &);
  void started(KIO::Job *);
  void frameDetached();
  virtual void didFirstLayout() {}
#endif

  int frameCount;
};

#if APPLE_CHANGES
#include "KWQKHTMLPart.h"
#endif

#endif
