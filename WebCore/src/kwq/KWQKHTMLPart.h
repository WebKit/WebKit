/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#ifndef __khtml_part_h__
#define __khtml_part_h__
 
#include <dom/html_document.h>
#include <dom/dom2_range.h>

#include <qregexp.h>

#include <kparts/part.h>
#include <kparts/browserextension.h>

#include <khtml_events.h>

#include <kjs_proxy.h>

class KHTMLSettings;
class KJavaAppletContext;
class KJSProxy;
class KHTMLPartPrivate;

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
@class IFWebDataSource;
#endif

namespace DOM
{
  class HTMLDocument;
  class HTMLDocumentImpl;
  class DocumentImpl;
  class Node;
};

namespace khtml
{
  class RenderPart;
  struct ChildFrame;
};

extern "C" {
    KJSProxy *kjs_html_init(KHTMLPart *khtmlpart);
}
 
class KHTMLPart : public KParts::ReadOnlyPart		// a.k.a. WebPageDocument
{
public:
  /**
   * Construct a new @ref KHTMLPart.
   *
   * KHTML basically consists of two objects: The @ref KHTMLPart itself,
   * holding the document data (DOM document), and the @ref KHTMLView,
   * derived from @ref QScrollview, in which the document content is
   * rendered in. You can specify two different parent objects for a
   * @ref KHTMLPart, one parent for the @ref KHTMLPart document and on parent
   * for the @ref KHTMLView. If the second @p parent argument is 0L, then
   * @p parentWidget is used as parent for both objects, the part and
   * the view.
   */
  KHTMLPart();

    // Perhaps new constructor?
  KHTMLPart(const KURL &url );

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

  /**
   * Stop loading the document and kill all data requests (for images, etc.)
   */
  virtual bool closeURL();

  /**
   * Retrieve a reference to the DOM HTML document (for non-HTML documents, returns null)
   */
  DOM::HTMLDocument htmlDocument() const;

  /**
   * Retrieve a reference to the DOM document.
   */
  DOM::Document document() const;

  /**
   * Retrieve the node that has the keyboard focus
   */
   // DUBIOUS, selection and focus should be handled externally
  DOM::Node activeNode() const;

  /**
   * Enable/disable Javascript support. Note that this will
   * in either case permanently override the default usersetting.
   * If you want to have the default UserSettings, don't call this
   * method.
   */
  void setJScriptEnabled( bool enable );

  /**
   * Deprecated, use setJScriptEnabled instead.
   */
  void enableJScript( bool enable ); // ### KDE 3.0: removeme

  /**
   * Returns @p true if Javascript support is enabled or @p false
   * otherwise.
   */
  bool jScriptEnabled() const;

  /**
   * Enable/disable the automatic forwarding by <meta http-equiv="refresh" ....>
   */
  void enableMetaRefresh( bool enable );

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
   // DUBIOUS, rather than executing the script this document should be
   // passed to the interpreter.
  QVariant executeScript( const QString &script );

  /**
   * Same as above except the Node parameter specifying the 'this' value.
   */
   // DUBIOUS, rather than executing the script this document should be
   // passed to the interpreter.
  QVariant executeScript( const DOM::Node &n, const QString &script );

  /**
   * Enable/disable Java applet support. Note that calling this function
   * will permanently override the User settings about Java applet support.
   * Not calling this function is the only way to let the default settings
   * apply.
   */
  void setJavaEnabled( bool enable );
  /**
   * Deprecated, use setJavaEnabled instead.
   */
  void enableJava( bool enable ); // ### KDE 3.0: removeme

  /**
   * Return if Java applet support is enabled/disabled.
   */
  bool javaEnabled() const;

  /**
   * Return the java context of the applets. If no applet exists, 0 is returned.
   */
  KJavaAppletContext *javaContext();

  /**
   * Return the java context of the applets. If no context exists yet, a new one is
   * created.
   */
  KJavaAppletContext *createJavaContext();

  /**
   * Deprecated. Use setPluginsEnabled instead.
   */
  void enablePlugins( bool enable ); // ### KDE 3.0: removeme

  /**
   * Enable or disable plugins via, default is enabled
   */
  void setPluginsEnabled( bool enable );

  /**
   * Return if plugins are enabled/disabled.
   */
  bool pluginsEnabled() const;

  /**
   * Deprecated. Use setAutoloadImages instead.
   */
  void autoloadImages( bool enable ); // ### KDE 3.0: removeme
  /**
   * Specify whether images contained in the document should be loaded
   * automatically or not.
   *
   * @note Request will be ignored if called before @ref begin().
   */
  void setAutoloadImages( bool enable );
  /**
   * Return whether images contained in the document are loaded automatically
   * or not.
   * @note that the returned information is unrelieable as long as no begin()
   * was called.
   */
  bool autoloadImages() const;

  /**
   * Security option
   *
   * Specify whether only local references ( stylesheets, images, scripts, subdocuments )
   * should be loaded. ( default false - everything is loaded, if the more specific
   * options allow )
   */
  void setOnlyLocalReferences(bool enable);

  /**
   * Return whether references should be loaded ( default false )
   **/
  bool onlyLocalReferences() const;

  /**
   * Clear the widget and prepares it for new content.
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
   * Write another part of the HTML code to the widget.
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
   * Write another part of the HTML code to the widget.
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

  /* Mainly used internally.
   *
   * Sets the document's base URL.
   */
  void setBaseURL( const KURL &url );

  /**
   * Retrieve the base URL of this document
   *
   * The base URL is ususally set by a <base url=...>
   * tag in the document head. If no base tag is set, the url of the current
   * document serves as base url and is returned.
   */
  KURL baseURL() const;

  /**
   * Mainly used internally.
   *
   *Sets the document's base target.
   */
  void setBaseTarget( const QString &target );

  /**
   * Retrieve the base target of this document.
   *
   * The base target is ususally set by a <base target=...>
   * tag in the document head.
   */
  QString baseTarget() const;

  /**
   * Set the charset to use for displaying HTML pages.
   *
   * If override is @p true,
   * it will override charset specifications of the document.
   */
  bool setCharset( const QString &name, bool override = false );

  /**
   * Set the encoding the page uses.
   *
   * This can be different from the charset. The widget will try to reload the current page in the new
   * encoding, if url() is not empty.
   */
  bool setEncoding( const QString &name, bool override = false );

  /**
   * return the encoding the page currently uses.
   *
   * Note that the encoding might be different from the charset.
   */
  QString encoding(); // ### KDE 3.0: make const

  /**
   * Set a user defined style sheet to be used on top of the HTML 4
   * default style sheet.
   *
   * This gives a wide range of possibilities to
   * change the layout of the page.
   */
  void setUserStyleSheet(const KURL &url);

  /**
   * Set a user defined style sheet to be used on top of the HTML 4
   * default style sheet.
   *
   * This gives a wide range of possibilities to
   * change the layout of the page.
   */
  void setUserStyleSheet(const QString &styleSheet);

  /**
   * Set point sizes to be associated with the HTML-sizes used in
   * <FONT size=Html-Font-Size>
   *
   * Html-Font-Sizes range from 0 (smallest) to 6 (biggest), but you
   * can specify up to 15 font sizes, the bigger ones will get used,
   * if <font size=+1> extends over 7, or if a 'font-size: larger'
   * style declaration gets into this region.
   *
   * They are related to the CSS font sizes by 0 == xx-small to 6 == xx-large.  */
  void setFontSizes(const QValueList<int> &newFontSizes );

  /**
   * Get point sizes to be associated with the HTML-sizes used in
   * <FONT size=Html-Font-Size>
   *
   * Html-Font-Sizes range from 0 (smallest) to 6 (biggest).
   *
   * They are related to the CSS font sizes by 0 == xx-small to 6 == xx-large.
   */
  QValueList<int> fontSizes() const;

  /**
   * Reset the point sizes to be associated with the HTML-sizes used in
   * <FONT size=Html-Font-Size> to their default.
   *
   * Html-Font-Sizes range from 0 (smallest) to 6 (biggest).
   */
  void resetFontSizes();

  /**
   * Set the standard font style.
   *
   * @param name The font name to use for standard text.
   */
  void setStandardFont( const QString &name );

  /**
   * Set the fixed font style.
   *
   * @param name The font name to use for fixed text, e.g.
   * the <tt>&lt;pre&gt;</tt> tag.
   */
  void setFixedFont( const QString &name );

  /**
   * Find the anchor named @p name.
   *
   * If the anchor is found, the widget
   * scrolls to the closest position. Returns @p if the anchor has
   * been found.
   */
    // DUBIOUS, this should be handled by the view, also isn't the anchor a node?
  bool gotoAnchor( const QString &name );

  /**
   * Set the cursor to use when the cursor is on a link.
   */
  void setURLCursor( const QCursor &c );

  /**
   * Retrieve the cursor which is used when the cursor is on a link.
   */
  const QCursor& urlCursor() const; // ### KDE 3.0: change return type to plain QCursor

  /**
   * Initiate a text search.
   */
    // DUBIOUS, perhaps searching should be handled externally
  void findTextBegin();

  /**
   * Find the next occurrance of the expression.
   */
    // DUBIOUS, perhaps searching should be handled externally
  bool findTextNext( const QRegExp &exp, bool forward );

  /**
   * Find the next occurence of the string.
   */
    // DUBIOUS, perhaps searching should be handled externally
  bool findTextNext( const QString &str, bool forward, bool caseSensitive );

  /**
   * Get the text the user has marked.
   */
    // DUBIOUS, perhaps selection should be managed externally
  virtual QString selectedText() const;

  /**
   * Retrieve the selected part of the HTML.
   */
    // DUBIOUS, perhaps selection should be managed externally
  DOM::Range selection() const;

  /**
   * set the current selection
   */
    // DUBIOUS, perhaps selection should be managed externally
  void setSelection( const DOM::Range & );

  /**
   * Has the user selected anything?
   *
   *  Call @ref selectedText() to
   * retrieve the selected text.
   *
   * @return @p true if there is text selected.
   */
    // DUBIOUS, perhaps selection should be managed externally
  bool hasSelection() const;

  /**
   * Marks all text in the document as selected.
   */
    // DUBIOUS, perhaps selection should be managed externally
  void selectAll();

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
    
    // Most of the following should be NOT be called, but must be implemented because
    // of existing references.

    // This should be private.
    const KHTMLSettings *settings() const;
    
    // This should be private.
    KJSProxy *jScript();

    // This should be private.
    KURL completeURL( const QString &url, const QString &target = QString::null );

    // This should be private.

    // This should be private.
    const KURL & url() const;

    // The following are only present to get khtml to build.
    void scheduleRedirection( int delay, const QString &url ); // ### KDE 3.0: make private?
    KHTMLView *view() const;
    void setView(KHTMLView *view);
    QWidget *widget();
    KHTMLPart *opener();
    KHTMLPart *parentPart();
    DOM::DocumentImpl *xmlDocImpl() const;
    const QList<KParts::ReadOnlyPart> frames() const;
    KHTMLPart *findFrame( const QString &f );
    void setOpener(KHTMLPart *_opener);
    bool openedByJS();
    void setOpenedByJS(bool _openedByJS);
    KParts::BrowserExtension *browserExtension() const;
    DOM::EventListener *createHTMLEventListener( QString code );
    QString requestFrameName();
    bool frameExists( const QString &frameName );
    bool requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                        const QStringList &args = QStringList(), bool isIFrame = false );
    void emitUnloadEvent();
    virtual void submitForm( const char *action, const QString &url, const QByteArray &formData,
                            const QString &target, const QString& contentType = QString::null,
                            const QString& boundary = QString::null ); // ### KDE 3.0: make private
    virtual void urlSelected( const QString &url, int button = 0, int state = 0,
                            const QString &_target = QString::null ); // ### KDE 3.0: make private
    bool requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                        const QStringList &args = QStringList() );

    void nodeActivated(const DOM::Node &);
    QVariant executeScheduledScript();
    void stopAutoScroll();
    virtual void overURL( const QString &url, const QString &target ); // ### KDE 3.0: make private (merge)

    bool event( QEvent *event );
    void khtmlMousePressEvent( khtml::MousePressEvent *event );
    void khtmlMouseDoubleClickEvent( khtml::MouseDoubleClickEvent * );
    void khtmlMouseMoveEvent( khtml::MouseMoveEvent *event );
    void khtmlMouseReleaseEvent( khtml::MouseReleaseEvent *event );
    void khtmlDrawContentsEvent( khtml::DrawContentsEvent * );


#ifdef _KWQ_
    QString documentSource();
    
    void init();
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    void slotData(id handle, const char *bytes, int length);  
#else
    void slotData(void *handle, const char *bytes, int length);  
#endif
#endif

    // this function checks to see whether a base URI and all its
    // associated sub-URIs have loaded
    void checkCompleted();

#ifdef _KWQ_
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    // Not retained.
    void setDataSource(IFWebDataSource *d) { dataSource = d; }
    IFWebDataSource *getDataSource() { return dataSource; }
#else
    void setDataSource(void *d) { dataSource = d; }
    void *getDataSource() { return dataSource; }
#endif
#endif

private:

    KHTMLPartPrivate *d;
    // DUBIOUS, why are impls being referenced?
    DOM::HTMLDocumentImpl *docImpl() const;    

#ifdef _KWQ_
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    IFWebDataSource *dataSource;
    QValueList<QString> plugins;
#else    
    void *dataSource;
#endif
#endif
};


#endif
