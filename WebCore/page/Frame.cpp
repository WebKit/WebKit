/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
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

#include "config.h"
#include "Frame.h"
#include "FramePrivate.h"

#include "ApplyStyleCommand.h"
#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "DOMImplementationImpl.h"
#include "DocLoader.h"
#include "EditingTextImpl.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCollectionImpl.h"
#include "HTMLFormElementImpl.h"
#include "HTMLGenericFormElementImpl.h"
#include "MouseEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "NodeListImpl.h"
#include "PlatformString.h"
#include "Plugin.h"
#include "RenderBlock.h"
#include "RenderText.h"
#include "SegmentedString.h"
#include "TypingCommand.h"
#include "VisiblePosition.h"
#include "css_computedstyle.h"
#include "css_valueimpl.h"
#include "cssproperties.h"
#include "cssstyleselector.h"
#include "dom2_eventsimpl.h"
#include "dom2_rangeimpl.h"
#include "html_baseimpl.h"
#include "html_documentimpl.h"
#include "html_imageimpl.h"
#include "html_objectimpl.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "khtml_settings.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "loader.h"
#include "markup.h"
#include "render_canvas.h"
#include "render_frames.h"
#include "render_layer.h"
#include "render_theme.h"
#include "visible_text.h"
#include "visible_units.h"
#include "xml_tokenizer.h"
#include "xmlhttprequest.h"
#include <assert.h>
#include <kio/global.h>
#include "TransferJob.h"
#include <kxmlcore/Assertions.h>
#include <qptrlist.h>
#include "TextEncoding.h"
#include <sys/types.h>
#include <math.h>

#if !WIN32
#include <unistd.h>
#endif

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGDocumentExtensions.h"
#endif

using namespace KJS;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const double caretBlinkFrequency = 0.5;
const double autoscrollInterval = 0.1;

class UserStyleSheetLoader : public CachedObjectClient {
public:
    UserStyleSheetLoader(Frame* frame, const String& url, DocLoader* dl)
        : m_frame(frame)
        , m_cachedSheet(Cache::requestStyleSheet(dl, url))
    {
        m_cachedSheet->ref(this);
    }
    ~UserStyleSheetLoader()
    {
        m_cachedSheet->deref(this);
    }
private:
    virtual void setStyleSheet(const String&, const String &sheet)
    {
        m_frame->setUserStyleSheet(sheet.qstring());
    }
    Frame* m_frame;
    CachedCSSStyleSheet* m_cachedSheet;
};

#if !NDEBUG
struct FrameCounter { 
    static int count; 
    ~FrameCounter() { if (count != 0) fprintf(stderr, "LEAK: %d Frame\n", count); }
};
int FrameCounter::count = 0;
static FrameCounter frameCounter;
#endif

static inline Frame* parentFromOwnerRenderer(RenderPart* ownerRenderer)
{
    if (!ownerRenderer)
        return 0;
    return ownerRenderer->node()->getDocument()->frame();
}

Frame::Frame(Page* page, RenderPart* ownerRenderer) 
    : d(new FramePrivate(page, parentFromOwnerRenderer(ownerRenderer), this, ownerRenderer))
    , _drawSelectionOnly(false)
    , m_markedTextUsesUnderlines(false)
    , m_windowHasFocus(false)
    , frameCount(0)
{
    AtomicString::init();
    Cache::init();
    EventNames::init();
    HTMLNames::init();
    QualifiedName::init();

#if SVG_SUPPORT
    SVGNames::init();
    XLinkNames::init();
#endif

    // FIXME: Frames were originally created with a refcount of 1, leave this
    // ref call here until we can straighten that out.
    ref();
#if !NDEBUG
    ++FrameCounter::count;
#endif

    if (ownerRenderer)
        ownerRenderer->setFrame(this);
}

Frame::~Frame()
{
    ASSERT(!d->m_lifeSupportTimer.isActive());

#if !NDEBUG
    --FrameCounter::count;
#endif

    cancelRedirection();

    if (!d->m_bComplete)
        closeURL();

    clear(false);

    if (d->m_jscript && d->m_jscript->haveInterpreter())
        if (Window* w = Window::retrieveWindow(this)) {
            w->disconnectFrame();
            // Must clear the window pointer, otherwise we will not
            // garbage-collect collect the window (inside the call to
            // delete d below).
            w = 0;
        }

    setOpener(0);
    HashSet<Frame*> openedBy = d->m_openedFrames;
    HashSet<Frame*>::iterator end = openedBy.end();
    for (HashSet<Frame*>::iterator it = openedBy.begin(); it != end; ++it)
        (*it)->setOpener(0);

    if (d->m_ownerRenderer)
        d->m_ownerRenderer->setFrame(0);
    ASSERT(!d->m_ownerRenderer);

    if (d->m_view) {
        d->m_view->hide();
        d->m_view->m_frame = 0;
    }
  
    ASSERT(!d->m_lifeSupportTimer.isActive());

    delete d->m_userStyleSheetLoader;
    delete d;
    d = 0;
}

bool Frame::didOpenURL(const KURL &url)
{
  if (d->m_scheduledRedirection == locationChangeScheduledDuringLoad) {
    // A redirect was shceduled before the document was created. This can happen
    // when one frame changes another frame's location.
    return false;
  }
  
  cancelRedirection();
  
  // clear last edit command
  d->m_lastEditCommand = EditCommandPtr();
  
  URLArgs args( d->m_extension->urlArgs() );

  closeURL();

  if (args.reload)
     d->m_cachePolicy = KIO::CC_Refresh;
  else
     d->m_cachePolicy = KIO::CC_Verify;

  if (args.doPost() && url.protocol().startsWith("http")) {
      d->m_job = new TransferJob(this, "POST", url, args.postData);
      d->m_job->addMetaData("content-type", args.contentType() );
  } else
      d->m_job = new TransferJob(this, "GET", url);

  d->m_job->addMetaData(args.metaData());

  d->m_bComplete = false;
  d->m_bLoadingMainResource = true;
  d->m_bLoadEventEmitted = false;

  d->m_kjsStatusBarText = String();
  d->m_kjsDefaultStatusBarText = String();

  d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled(url.host());
  d->m_bJavaEnabled = d->m_settings->isJavaEnabled(url.host());
  d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled(url.host());

  // initializing d->m_url to the new url breaks relative links when opening such a link after this call and _before_ begin() is called (when the first
  // data arrives) (Simon)
  d->m_url = url;
  if(d->m_url.protocol().startsWith("http") && !d->m_url.host().isEmpty() && d->m_url.path().isEmpty())
    d->m_url.setPath("/");
  d->m_workingURL = d->m_url;

  started();

  return true;
}

void Frame::didExplicitOpen()
{
  d->m_bComplete = false;
  d->m_bLoadEventEmitted = false;
    
  // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
  // from a subsequent window.document.open / window.document.write call. 
  // Cancelling redirection here works for all cases because document.open 
  // implicitly precedes document.write.
  cancelRedirection(); 
}

void Frame::stopLoading(bool sendUnload)
{
  if (d->m_doc && d->m_doc->tokenizer())
    d->m_doc->tokenizer()->stopParsing();
    
  if (d->m_job)
  {
    d->m_job->kill();
    d->m_job = 0;
  }

  if (sendUnload) {
    if (d->m_doc) {
      if (d->m_bLoadEventEmitted && !d->m_bUnloadEventEmitted ) {
        d->m_doc->dispatchWindowEvent(unloadEvent, false, false);
        if (d->m_doc)
          d->m_doc->updateRendering();
        d->m_bUnloadEventEmitted = true;
      }
    }
    
    if (d->m_doc && !d->m_doc->inPageCache())
      d->m_doc->removeAllEventListenersFromAllNodes();
  }

  d->m_bComplete = true; // to avoid calling completed() in finishedParsing() (David)
  d->m_bLoadingMainResource = false;
  d->m_bLoadEventEmitted = true; // don't want that one either
  d->m_cachePolicy = KIO::CC_Verify; // Why here?

  if (d->m_doc && d->m_doc->parsing()) {
    finishedParsing();
    d->m_doc->setParsing(false);
  }
  
  d->m_workingURL = KURL();

  if (DocumentImpl *doc = d->m_doc.get()) {
    if (DocLoader *docLoader = doc->docLoader())
      Cache::loader()->cancelRequests(docLoader);
      XMLHttpRequest::cancelRequests(doc);
  }

  // tell all subframes to stop as well
  for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
      child->stopLoading(sendUnload);

  d->m_bPendingChildRedirection = false;

  cancelRedirection();
}

BrowserExtension *Frame::browserExtension() const
{
  return d->m_extension;
}

FrameView* Frame::view() const
{
    return d->m_view.get();
}

void Frame::setView(FrameView* view)
{
    d->m_view = view;
}

bool Frame::jScriptEnabled() const
{
  return d->m_bJScriptEnabled;
}

void Frame::setMetaRefreshEnabled( bool enable )
{
  d->m_metaRefreshEnabled = enable;
}

bool Frame::metaRefreshEnabled() const
{
  return d->m_metaRefreshEnabled;
}

KJSProxyImpl *Frame::jScript()
{
    if (!d->m_bJScriptEnabled)
        return 0;

    if (!d->m_jscript)
        d->m_jscript = new KJSProxyImpl(this);

    return d->m_jscript;
}

static bool getString(JSValue* result, QString& string)
{
    if (!result)
        return false;
    JSLock lock;
    UString ustring;
    if (!result->getString(ustring))
        return false;
    string = ustring.qstring();
    return true;
}

void Frame::replaceContentsWithScriptResult(const KURL& url)
{
    JSValue* ret = executeScript(0, KURL::decode_string(url.url().mid(strlen("javascript:"))));
    QString scriptResult;
    if (getString(ret, scriptResult)) {
        begin();
        write(scriptResult);
        end();
    }
}

JSValue* Frame::executeScript(NodeImpl* n, const QString& script, bool forceUserGesture)
{
  KJSProxyImpl *proxy = jScript();

  if (!proxy)
    return 0;

  d->m_runningScripts++;
  // If forceUserGesture is true, then make the script interpreter
  // treat it as if triggered by a user gesture even if there is no
  // current DOM event being processed.
  JSValue* ret = proxy->evaluate(forceUserGesture ? QString::null : d->m_url.url(), 0, script, n);
  d->m_runningScripts--;

  if (!d->m_runningScripts)
      submitFormAgain();

  DocumentImpl::updateDocumentsRendering();

  return ret;
}

bool Frame::javaEnabled() const
{
#ifndef Q_WS_QWS
  return d->m_bJavaEnabled;
#else
  return false;
#endif
}

bool Frame::pluginsEnabled() const
{
  return d->m_bPluginsEnabled;
}

void Frame::setAutoloadImages( bool enable )
{
  if ( d->m_doc && d->m_doc->docLoader()->autoloadImages() == enable )
    return;

  if ( d->m_doc )
    d->m_doc->docLoader()->setAutoloadImages( enable );
}

bool Frame::autoloadImages() const
{
  if ( d->m_doc )
    return d->m_doc->docLoader()->autoloadImages();

  return true;
}

void Frame::clear(bool clearWindowProperties)
{
  if ( d->m_bCleared )
    return;
  d->m_bCleared = true;
  d->m_mousePressNode = 0;

  if (d->m_doc) {
    d->m_doc->cancelParsing();
    d->m_doc->detach();
  }

  // Moving past doc so that onUnload works.
  if (clearWindowProperties && d->m_jscript)
    d->m_jscript->clear();

  if ( d->m_view )
    d->m_view->clear();

  // do not drop the document before the jscript and view are cleared, as some destructors
  // might still try to access the document.
  d->m_doc = 0;
  d->m_decoder = 0;

  d->m_plugins.clear();

  d->m_scheduledRedirection = noRedirectionScheduled;
  d->m_delayRedirect = 0;
  d->m_redirectURL = QString::null;
  d->m_redirectReferrer = QString::null;
  d->m_redirectLockHistory = true;
  d->m_redirectUserGesture = false;
  d->m_bHTTPRefresh = false;
  d->m_bFirstData = true;

  d->m_bMousePressed = false;

  if ( !d->m_haveEncoding )
    d->m_encoding = QString::null;
}

DocumentImpl *Frame::document() const
{
    if (d)
        return d->m_doc.get();
    return 0;
}

void Frame::setDocument(DocumentImpl* newDoc)
{
    if (d) {
        if (d->m_doc)
            d->m_doc->detach();
        d->m_doc = newDoc;
        if (newDoc)
            newDoc->attach();
    }
}

void Frame::receivedFirstData()
{
    begin( d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset );

    d->m_doc->docLoader()->setCachePolicy(d->m_cachePolicy);
    d->m_workingURL = KURL();

    // When the first data arrives, the metadata has just been made available
    QString qData;

    // Support for http-refresh
    qData = d->m_job->queryMetaData("http-refresh");
    if(!qData.isEmpty() && d->m_metaRefreshEnabled) {
      double delay;
      int pos = qData.find( ';' );
      if ( pos == -1 )
        pos = qData.find( ',' );

      if( pos == -1 )
      {
        delay = qData.stripWhiteSpace().toDouble();
        // We want a new history item if the refresh timeout > 1 second
        scheduleRedirection( delay, d->m_url.url(), delay <= 1);
      }
      else
      {
        int end_pos = qData.length();
        delay = qData.left(pos).stripWhiteSpace().toDouble();
        while ( qData[++pos] == ' ' );
        if ( qData.find( "url", pos, false ) == pos )
        {
          pos += 3;
          while (qData[pos] == ' ' || qData[pos] == '=' )
              pos++;
          if ( qData[pos] == '"' )
          {
              pos++;
              int index = end_pos-1;
              while( index > pos )
              {
                if ( qData[index] == '"' )
                    break;
                index--;
              }
              if ( index > pos )
                end_pos = index;
          }
        }
        // We want a new history item if the refresh timeout > 1 second
        scheduleRedirection( delay, d->m_doc->completeURL( qData.mid( pos, end_pos ) ), delay <= 1);
      }
      d->m_bHTTPRefresh = true;
    }

    // Support for http last-modified
    d->m_lastModified = d->m_job->queryMetaData("modified");
}

void Frame::receivedAllData(TransferJob* job)
{
    d->m_job = 0;

    if (job->error()) {
        checkCompleted();
        return;
    }

    d->m_workingURL = KURL();

    if (d->m_doc->parsing())
        end(); // will call completed()
}

void Frame::childBegin()
{
    // We need to do this when the child is created so as to avoid the parent thining the child
    // is complete before it has even started loading.
    // FIXME: do we really still need this?
    d->m_bComplete = false;
}

void Frame::begin( const KURL &url, int xOffset, int yOffset )
{
  if (d->m_workingURL.isEmpty())
    createEmptyDocument(); // Creates an empty document if we don't have one already

  clear();
  partClearedInBegin();

  d->m_bCleared = false;
  d->m_bComplete = false;
  d->m_bLoadEventEmitted = false;
  d->m_bLoadingMainResource = true;

  URLArgs args( d->m_extension->urlArgs() );
  args.xOffset = xOffset;
  args.yOffset = yOffset;
  d->m_extension->setURLArgs( args );

  KURL ref(url);
  ref.setUser(QSTRING_NULL);
  ref.setPass(QSTRING_NULL);
  ref.setRef(QSTRING_NULL);
  d->m_referrer = ref.url();
  d->m_url = url;
  KURL baseurl;

  // We don't need KDE chained URI handling or window caption setting
  if (!d->m_url.isEmpty())
    baseurl = d->m_url;

  if (DOMImplementationImpl::isXMLMIMEType(args.serviceType))
    d->m_doc = DOMImplementationImpl::instance()->createDocument(d->m_view.get());
  else
    d->m_doc = DOMImplementationImpl::instance()->createHTMLDocument(d->m_view.get());

  if (!d->m_doc->attached())
    d->m_doc->attach( );
  d->m_doc->setURL( d->m_url.url() );
  // We prefer m_baseURL over d->m_url because d->m_url changes when we are
  // about to load a new page.
  d->m_doc->setBaseURL( baseurl.url() );
  if (d->m_decoder)
    d->m_doc->setDecoder(d->m_decoder.get());
  d->m_doc->docLoader()->setShowAnimations( d->m_settings->showAnimations() );

  updatePolicyBaseURL();

  setAutoloadImages( d->m_settings->autoLoadImages() );
  QString userStyleSheet = d->m_settings->userStyleSheet();

  if ( !userStyleSheet.isEmpty() )
    setUserStyleSheet( KURL( userStyleSheet ) );

  restoreDocumentState();

  d->m_doc->implicitOpen();
  // clear widget
  if (d->m_view)
    d->m_view->resizeContents( 0, 0 );
}

void Frame::write(const char* str, int len)
{
    if ( !d->m_decoder ) {
        d->m_decoder = new Decoder;
        if (!d->m_encoding.isNull())
            d->m_decoder->setEncodingName(d->m_encoding.latin1(),
                d->m_haveEncoding ? Decoder::UserChosenEncoding : Decoder::EncodingFromHTTPHeader);
        else
            d->m_decoder->setEncodingName(settings()->encoding().latin1(), Decoder::DefaultEncoding);

        if (d->m_doc)
            d->m_doc->setDecoder(d->m_decoder.get());
    }
  if ( len == 0 )
    return;

  if ( len == -1 )
    len = strlen( str );

  QString decoded = d->m_decoder->decode( str, len );

  if(decoded.isEmpty()) return;

  if(d->m_bFirstData) {
      // determine the parse mode
      d->m_doc->determineParseMode( decoded );
      d->m_bFirstData = false;

      // ### this is still quite hacky, but should work a lot better than the old solution
      if(d->m_decoder->visuallyOrdered()) d->m_doc->setVisuallyOrdered();
      d->m_doc->recalcStyle( NodeImpl::Force );
  }

  if (Tokenizer* t = d->m_doc->tokenizer())
      t->write( decoded, true );
}

void Frame::write( const QString &str )
{
  if ( str.isNull() )
    return;

  if(d->m_bFirstData) {
      // determine the parse mode
      d->m_doc->setParseMode( DocumentImpl::Strict );
      d->m_bFirstData = false;
  }
  Tokenizer* t = d->m_doc->tokenizer();
  if(t)
    t->write( str, true );
}

void Frame::end()
{
    d->m_bLoadingMainResource = false;
    endIfNotLoading();
}

void Frame::endIfNotLoading()
{
    if (d->m_bLoadingMainResource)
        return;

    // make sure nothing's left in there...
    if (d->m_decoder)
        write(d->m_decoder->flush());
    if (d->m_doc)
        d->m_doc->finishParsing();
    else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
}

void Frame::stop()
{
    if (d->m_doc) {
        if (d->m_doc->tokenizer())
            d->m_doc->tokenizer()->stopParsing();
        d->m_doc->finishParsing();
    } else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
}

void Frame::stopAnimations()
{
  if (d->m_doc)
      d->m_doc->docLoader()->setShowAnimations(KHTMLSettings::KAnimationDisabled);

  for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
      child->stopAnimations();
}

void Frame::gotoAnchor()
{
    // If our URL has no ref, then we have no place we need to jump to.
    if (!d->m_url.hasRef())
        return;

    QString ref = d->m_url.encodedHtmlRef();
    if (!gotoAnchor(ref)) {
        // Can't use htmlRef() here because it doesn't know which encoding to use to decode.
        // Decoding here has to match encoding in completeURL, which means it has to use the
        // page's encoding rather than UTF-8.
        if (d->m_decoder)
            gotoAnchor(KURL::decode_string(ref, d->m_decoder->encoding()));
    }
}

void Frame::finishedParsing()
{
  RefPtr<Frame> protector(this);
  checkCompleted();

  if (!d->m_view)
    return; // We are being destroyed by something checkCompleted called.

  // check if the scrollbars are really needed for the content
  // if not, remove them, relayout, and repaint

  d->m_view->restoreScrollBar();
  gotoAnchor();
}

void Frame::loadDone()
{
    if (d->m_doc)
        checkCompleted();
}

void Frame::checkCompleted()
{
  // Any frame that hasn't completed yet ?
  for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
      if (!child->d->m_bComplete)
          return;

  // Have we completed before?
  if (d->m_bComplete)
      return;

  // Are we still parsing?
  if (d->m_doc && d->m_doc->parsing())
      return;

  // Still waiting for images/scripts from the loader ?
  int requests = 0;
  if (d->m_doc && d->m_doc->docLoader())
      requests = Cache::loader()->numRequests(d->m_doc->docLoader());

  if (requests > 0)
      return;

  // OK, completed.
  // Now do what should be done when we are really completed.
  d->m_bComplete = true;

  checkEmitLoadEvent(); // if we didn't do it before

  if (d->m_scheduledRedirection != noRedirectionScheduled) {
      // Do not start redirection for frames here! That action is
      // deferred until the parent emits a completed signal.
      if (!tree()->parent())
          startRedirectionTimer();

      completed(true);
  } else {
      completed(d->m_bPendingChildRedirection);
  }
}

void Frame::checkEmitLoadEvent()
{
    if (d->m_bLoadEventEmitted || !d->m_doc || d->m_doc->parsing())
        return;

    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        if (!child->d->m_bComplete) // still got a frame running -> too early
            return;

    // All frames completed -> set their domain to the frameset's domain
    // This must only be done when loading the frameset initially (#22039),
    // not when following a link in a frame (#44162).
    if (d->m_doc) {
        DOMString domain = d->m_doc->domain();
        for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
            if (child->d->m_doc)
                child->d->m_doc->setDomain(domain);
    }

    d->m_bLoadEventEmitted = true;
    d->m_bUnloadEventEmitted = false;
    if (d->m_doc)
        d->m_doc->implicitClose();
}

const KHTMLSettings *Frame::settings() const
{
  return d->m_settings;
}

KURL Frame::baseURL() const
{
    if (!d->m_doc)
        return KURL();
    return d->m_doc->baseURL();
}

QString Frame::baseTarget() const
{
    if (!d->m_doc)
        return QString();
    return d->m_doc->baseTarget();
}

KURL Frame::completeURL( const QString &url )
{
    if (!d->m_doc)
        return url;

    return KURL(d->m_doc->completeURL(url));
}

void Frame::scheduleRedirection( double delay, const QString &url, bool doLockHistory)
{
    if (delay < 0 || delay > INT_MAX / 1000)
      return;
    if ( d->m_scheduledRedirection == noRedirectionScheduled || delay <= d->m_delayRedirect )
    {
       d->m_scheduledRedirection = redirectionScheduled;
       d->m_delayRedirect = delay;
       d->m_redirectURL = url;
       d->m_redirectReferrer = QString::null;
       d->m_redirectLockHistory = doLockHistory;
       d->m_redirectUserGesture = false;

       stopRedirectionTimer();
       if (d->m_bComplete)
         startRedirectionTimer();
    }
}

void Frame::scheduleLocationChange(const QString &url, const QString &referrer, bool lockHistory, bool userGesture)
{
    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    d->m_scheduledRedirection = d->m_doc ? locationChangeScheduled : locationChangeScheduledDuringLoad;
    
    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (d->m_scheduledRedirection == locationChangeScheduledDuringLoad) {
        stopLoading(true);   
    }
    
    d->m_delayRedirect = 0;
    d->m_redirectURL = url;
    d->m_redirectReferrer = referrer;
    d->m_redirectLockHistory = lockHistory;
    d->m_redirectUserGesture = userGesture;
    stopRedirectionTimer();
    if (d->m_bComplete)
        startRedirectionTimer();
}

bool Frame::isScheduledLocationChangePending() const
{
    switch (d->m_scheduledRedirection) {
        case noRedirectionScheduled:
        case redirectionScheduled:
            return false;
        case historyNavigationScheduled:
        case locationChangeScheduled:
        case locationChangeScheduledDuringLoad:
            return true;
    }
    return false;
}

void Frame::scheduleHistoryNavigation( int steps )
{
    // navigation will always be allowed in the 0 steps case, which is OK because
    // that's supposed to force a reload.
    if (!canGoBackOrForward(steps)) {
        cancelRedirection();
        return;
    }

    d->m_scheduledRedirection = historyNavigationScheduled;
    d->m_delayRedirect = 0;
    d->m_redirectURL = QString::null;
    d->m_redirectReferrer = QString::null;
    d->m_scheduledHistoryNavigationSteps = steps;
    stopRedirectionTimer();
    if (d->m_bComplete)
        startRedirectionTimer();
}

void Frame::cancelRedirection(bool cancelWithLoadInProgress)
{
    if (d) {
        d->m_cancelWithLoadInProgress = cancelWithLoadInProgress;
        d->m_scheduledRedirection = noRedirectionScheduled;
        stopRedirectionTimer();
    }
}

void Frame::changeLocation(const QString &URL, const QString &referrer, bool lockHistory, bool userGesture)
{
    if (URL.find("javascript:", 0, false) == 0) {
        QString script = KURL::decode_string(URL.mid(11));
        JSValue* result = executeScript(0, script, userGesture);
        QString scriptResult;
        if (getString(result, scriptResult)) {
            begin(url());
            write(scriptResult);
            end();
        }
        return;
    }

    URLArgs args;

    args.setLockHistory(lockHistory);
    if (!referrer.isEmpty())
        args.metaData().set("referrer", referrer);

    urlSelected(URL, "_self", args);
}

void Frame::redirectionTimerFired(Timer<Frame>*)
{
    if (d->m_scheduledRedirection == historyNavigationScheduled) {
        d->m_scheduledRedirection = noRedirectionScheduled;

        // Special case for go(0) from a frame -> reload only the frame
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla).... we can't easily do that
        // in Konqueror...
        if (d->m_scheduledHistoryNavigationSteps == 0) // add && parent() to get only frames, but doesn't matter
            openURL( url() ); /// ## need args.reload=true?
        else {
            if (d->m_extension) {
                d->m_extension->goBackOrForward(d->m_scheduledHistoryNavigationSteps);
            }
        }
        return;
    }

    QString URL = d->m_redirectURL;
    QString referrer = d->m_redirectReferrer;
    bool lockHistory = d->m_redirectLockHistory;
    bool userGesture = d->m_redirectUserGesture;

    d->m_scheduledRedirection = noRedirectionScheduled;
    d->m_delayRedirect = 0;
    d->m_redirectURL = QString::null;
    d->m_redirectReferrer = QString::null;

    changeLocation(URL, referrer, lockHistory, userGesture);
}

void Frame::receivedRedirect(TransferJob*, const KURL& url)
{
    d->m_workingURL = url;
}

QString Frame::encoding() const
{
    if (d->m_haveEncoding && !d->m_encoding.isEmpty())
        return d->m_encoding;

    if (d->m_decoder && d->m_decoder->encoding().isValid())
        return QString(d->m_decoder->encodingName());

    return settings()->encoding();
}

void Frame::setUserStyleSheet(const KURL& url)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc && d->m_doc->docLoader())
        d->m_userStyleSheetLoader = new UserStyleSheetLoader(this, url.url(), d->m_doc->docLoader());
}

void Frame::setUserStyleSheet(const QString& styleSheet)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc)
        d->m_doc->setUserStyleSheet(styleSheet);
}

bool Frame::gotoAnchor( const QString &name )
{
  if (!d->m_doc)
    return false;

  NodeImpl *n = d->m_doc->getElementById(name);
  if (!n) {
    HTMLCollectionImpl *anchors =
        new HTMLCollectionImpl(d->m_doc.get(), HTMLCollectionImpl::DOC_ANCHORS);
    anchors->ref();
    n = anchors->namedItem(name, !d->m_doc->inCompatMode());
    anchors->deref();
  }

  d->m_doc->setCSSTarget(n); // Setting to null will clear the current target.
  
  // Implement the rule that "" and "top" both mean top of page as in other browsers.
  if (!n && !(name.isEmpty() || name.lower() == "top"))
      return false;

  // We need to update the layout before scrolling, otherwise we could
  // really mess things up if an anchor scroll comes at a bad moment.
  if ( d->m_doc ) {
    d->m_doc->updateRendering();
    // Only do a layout if changes have occurred that make it necessary.      
    if ( d->m_view && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout() ) {
      d->m_view->layout();
    }
  }
  
  // Scroll nested layers and frames to reveal the anchor.
  RenderObject *renderer;
  IntRect rect;
  if (n) {
      renderer = n->renderer();
      rect = n->getRect();
  } else {
    // If there's no node, we should scroll to the top of the document.
      renderer = d->m_doc->renderer();
      rect = IntRect();
  }

  if (renderer) {
    // Align to the top and to the closest side (this matches other browsers).
    renderer->enclosingLayer()->scrollRectToVisible(rect, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);
  }
  
  return true;
}

void Frame::setStandardFont( const QString &name )
{
    d->m_settings->setStdFontName(name);
}

void Frame::setFixedFont( const QString &name )
{
    d->m_settings->setFixedFontName(name);
}

QString Frame::selectedText() const
{
    return plainText(selection().toRange().get());
}

bool Frame::hasSelection() const
{
    return d->m_selection.isCaretOrRange();
}

SelectionController &Frame::selection() const
{
    return d->m_selection;
}

ETextGranularity Frame::selectionGranularity() const
{
    return d->m_selectionGranularity;
}

void Frame::setSelectionGranularity(ETextGranularity granularity) const
{
    d->m_selectionGranularity = granularity;
}

const SelectionController &Frame::dragCaret() const
{
    return d->m_dragCaret;
}

const Selection& Frame::mark() const
{
    return d->m_mark;
}

void Frame::setMark(const Selection& s)
{
    d->m_mark = s;
}

void Frame::setSelection(const SelectionController &s, bool closeTyping, bool keepTypingStyle)
{
    if (d->m_selection == s) {
        return;
    }
    
    clearCaretRectIfNeeded();

    SelectionController oldSelection = d->m_selection;

    d->m_selection = s;
    if (!s.isNone())
        setFocusNodeIfNeeded();
    
    selectionLayoutChanged();

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    d->m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;

    if (closeTyping)
        TypingCommand::closeTyping(lastEditCommand());

    if (!keepTypingStyle)
        clearTypingStyle();
    
    respondToChangedSelection(oldSelection, closeTyping);
}

void Frame::setDragCaret(const SelectionController &dragCaret)
{
    if (d->m_dragCaret != dragCaret) {
        d->m_dragCaret.needsCaretRepaint();
        d->m_dragCaret = dragCaret;
        d->m_dragCaret.needsCaretRepaint();
    }
}

void Frame::invalidateSelection()
{
    clearCaretRectIfNeeded();
    d->m_selection.setNeedsLayout();
    selectionLayoutChanged();
}

void Frame::setCaretVisible(bool flag)
{
    if (d->m_caretVisible == flag)
        return;
    clearCaretRectIfNeeded();
    if (flag)
        setFocusNodeIfNeeded();
    d->m_caretVisible = flag;
    selectionLayoutChanged();
}


void Frame::clearCaretRectIfNeeded()
{
    if (d->m_caretPaint) {
        d->m_caretPaint = false;
        d->m_selection.needsCaretRepaint();
    }        
}

// Helper function that tells whether a particular node is an element that has an entire
// Frame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const NodeImpl *n)
{
    if (!n)
        return false;
    RenderObject *renderer = n->renderer();
    if (!renderer || !renderer->isWidget())
        return false;
    Widget *widget = static_cast<RenderWidget *>(renderer)->widget();
    return widget && widget->isFrameView();
}

void Frame::setFocusNodeIfNeeded()
{
    if (!document() || d->m_selection.isNone() || !d->m_isFocused)
        return;

    NodeImpl *startNode = d->m_selection.start().node();
    NodeImpl *target = startNode ? startNode->rootEditableElement() : 0;
    
    if (target) {
        RenderObject* renderer = target->renderer();

        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target && target->isMouseFocusable() && !isFrameElement(target)) {
                document()->setFocusNode(target);
                return;
            }
            renderer = renderer->parent();
            if (renderer)
                target = renderer->element();
        }
        document()->setFocusNode(0);
    }
}

void Frame::selectionLayoutChanged()
{
    // kill any caret blink timer now running
    d->m_caretBlinkTimer.stop();

    // see if a new caret blink timer needs to be started
    if (d->m_caretVisible && d->m_caretBlinks && 
        d->m_selection.isCaret() && d->m_selection.start().node()->isContentEditable()) {
        d->m_caretBlinkTimer.startRepeating(caretBlinkFrequency);
        d->m_caretPaint = true;
        d->m_selection.needsCaretRepaint();
    }

    if (d->m_doc)
        d->m_doc->updateSelection();
}

void Frame::setXPosForVerticalArrowNavigation(int x)
{
    d->m_xPosForVerticalArrowNavigation = x;
}

int Frame::xPosForVerticalArrowNavigation() const
{
    return d->m_xPosForVerticalArrowNavigation;
}

void Frame::caretBlinkTimerFired(Timer<Frame>*)
{
    // Might be better to turn the timer off during some of these circumstances
    // and assert rather then letting the timer fire and do nothing here.
    // Could do that in selectionLayoutChanged.

    if (!d->m_caretVisible)
        return;
    if (!d->m_caretBlinks)
        return;
    if (!d->m_selection.isCaret())
        return;
    bool caretPaint = d->m_caretPaint;
    if (d->m_bMousePressed && caretPaint)
        return;
    d->m_caretPaint = !caretPaint;
    d->m_selection.needsCaretRepaint();
}

void Frame::paintCaret(GraphicsContext* p, const IntRect &rect) const
{
    if (d->m_caretPaint)
        d->m_selection.paintCaret(p, rect);
}

void Frame::paintDragCaret(GraphicsContext* p, const IntRect &rect) const
{
    d->m_dragCaret.paintCaret(p, rect);
}

void Frame::urlSelected(const QString &url, const QString& _target, const URLArgs& args )
{
  QString target = _target;
  if (target.isEmpty() && d->m_doc)
    target = d->m_doc->baseTarget();

  if (url.startsWith("javascript:", false)) {
    executeScript(0, KURL::decode_string(url.mid(11)), true);
    return;
  }

  KURL cURL = completeURL(url);
  if (!cURL.isValid())
    // ### ERROR HANDLING
    return;

  URLArgs argsCopy = args;
  argsCopy.frameName = target;

  if (d->m_bHTTPRefresh) {
    d->m_bHTTPRefresh = false;
    argsCopy.metaData().set("cache", "refresh");
  }

  if (!d->m_referrer.isEmpty())
    argsCopy.metaData().set("referrer", d->m_referrer);

  urlSelected(cURL, argsCopy);
}

bool Frame::requestFrame(RenderPart* renderer, const QString& _url, const QString& frameName)
{
    // Support for <frame src="javascript:string">
    KURL scriptURL;
    KURL url;
    if (_url.startsWith("javascript:", false)) {
        scriptURL = _url;
        url = "about:blank";
    } else
        url = completeURL(_url);

    Frame* frame = tree()->child(frameName);
    if (frame) {
        URLArgs args;
        args.metaData().set("referrer", d->m_referrer);
        args.reload = (d->m_cachePolicy == KIO::CC_Reload) || (d->m_cachePolicy == KIO::CC_Refresh);
        frame->openURLRequest(url, args);
    } else
        frame = loadSubframe(renderer, url, frameName, d->m_referrer);
    
    if (!frame)
        return false;

    if (!scriptURL.isEmpty())
        frame->replaceContentsWithScriptResult(scriptURL);

    return true;
}

bool Frame::requestObject(RenderPart* renderer, const QString& url, const QString& frameName,
                          const QString& mimeType, const QStringList& paramNames, const QStringList& paramValues)
{
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);
    
    if (url.isEmpty() && mimeType.isEmpty())
        return true;
    
    bool useFallback;
    if (shouldUsePlugin(renderer->element(), completedURL, mimeType, renderer->hasFallbackContent(), useFallback))
        return loadPlugin(renderer, completedURL, mimeType, paramNames, paramValues, useFallback);

    // FIXME: ok to always make a new one? when does the old frame get removed?
    return loadSubframe(renderer, completedURL, frameName, d->m_referrer);
}

bool Frame::shouldUsePlugin(NodeImpl* element, const KURL& url, const QString& mimeType, bool hasFallback, bool& useFallback)
{
    useFallback = false;
    ObjectContentType objectType = objectContentType(url, mimeType);

    // if an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon
    if (objectType == ObjectContentNone && hasFallback)
        useFallback = true;

    return objectType == ObjectContentNone || objectType == ObjectContentPlugin;
}


bool Frame::loadPlugin(RenderPart *renderer, const KURL &url, const QString &mimeType, 
                       const QStringList& paramNames, const QStringList& paramValues, bool useFallback)
{
    if (useFallback) {
        checkEmitLoadEvent();
        return false;
    }

    Plugin* plugin = createPlugin(url, paramNames, paramValues, mimeType);
    if (!plugin) {
        checkEmitLoadEvent();
        return false;
    }
    d->m_plugins.append(plugin);
    
    if (renderer && plugin->view())
        renderer->setWidget(plugin->view());
    
    checkEmitLoadEvent();
    
    return true;
}

Frame* Frame::loadSubframe(RenderPart* renderer, const KURL& url, const QString& name, const DOMString& referrer)
{
    Frame* frame = createFrame(url, name, renderer, referrer);
    if (!frame)  {
        checkEmitLoadEvent();
        return 0;
    }
    
    frame->childBegin();
    
    if (renderer && frame->view())
        renderer->setWidget(frame->view());
    
    checkEmitLoadEvent();
    
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the 
    // completed() signal for the child by hand
    // FIXME: In this case the Frame will have finished loading before 
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (url.isEmpty() || url == "about:blank") {
        frame->completed(false);
        frame->checkCompleted();
    }

    return frame;
}

void Frame::submitFormAgain()
{
    FramePrivate::SubmitForm* form = d->m_submitForm;
    d->m_submitForm = 0;
    if (d->m_doc && !d->m_doc->parsing() && form)
        submitForm(form->submitAction, form->submitUrl, form->submitFormData,
            form->target, form->submitContentType, form->submitBoundary);
    delete form;
}

void Frame::submitForm( const char *action, const QString &url, const FormData &formData, const QString &_target, const QString& contentType, const QString& boundary )
{
  KURL u = completeURL( url );

  if (!u.isValid())
    // ### ERROR HANDLING!
    return;

  QString urlstring = u.url();
  if (urlstring.startsWith("javascript:", false)) {
    urlstring = KURL::decode_string(urlstring);
    d->m_executingJavaScriptFormAction = true;
    executeScript(0, urlstring.mid(11));
    d->m_executingJavaScriptFormAction = false;
    return;
  }

  URLArgs args;

  if (!d->m_referrer.isEmpty())
     args.metaData().set("referrer", d->m_referrer);

  args.frameName = _target.isEmpty() ? d->m_doc->baseTarget() : _target ;

  // Handle mailto: forms
  if (u.protocol() == "mailto") {
      // 1)  Check for attach= and strip it
      QString q = u.query().mid(1);
      QStringList nvps = QStringList::split("&", q);
      bool triedToAttach = false;

      for (QStringList::Iterator nvp = nvps.begin(); nvp != nvps.end(); ++nvp) {
         QStringList pair = QStringList::split("=", *nvp);
         if (pair.count() >= 2) {
            if (pair.first().lower() == "attach") {
               nvp = nvps.remove(nvp);
               triedToAttach = true;
            }
         }
      }


      // 2)  Append body=
      QString bodyEnc;
      if (contentType.lower() == "multipart/form-data") {
         // FIXME: is this correct?  I suspect not
         bodyEnc = KURL::encode_string(formData.flattenToString());
      } else if (contentType.lower() == "text/plain") {
         // Convention seems to be to decode, and s/&/\n/
         QString tmpbody = formData.flattenToString();
         tmpbody.replace('&', '\n');
         tmpbody.replace('+', ' ');
         tmpbody = KURL::decode_string(tmpbody);  // Decode the rest of it
         bodyEnc = KURL::encode_string(tmpbody);  // Recode for the URL
      } else {
         bodyEnc = KURL::encode_string(formData.flattenToString());
      }

      nvps.append(QString("body=%1").arg(bodyEnc));
      q = nvps.join("&");
      u.setQuery(q);
  } 

  if ( strcmp( action, "get" ) == 0 ) {
    if (u.protocol() != "mailto")
       u.setQuery( formData.flattenToString() );
    args.setDoPost( false );
  }
  else {
    args.postData = formData;
    args.setDoPost( true );

    // construct some user headers if necessary
    if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
      args.setContentType( "Content-Type: application/x-www-form-urlencoded" );
    else // contentType must be "multipart/form-data"
      args.setContentType( "Content-Type: " + contentType + "; boundary=" + boundary );
  }

  if ( d->m_doc->parsing() || d->m_runningScripts > 0 ) {
    if(d->m_submitForm)
        return;
    d->m_submitForm = new FramePrivate::SubmitForm;
    d->m_submitForm->submitAction = action;
    d->m_submitForm->submitUrl = url;
    d->m_submitForm->submitFormData = formData;
    d->m_submitForm->target = _target;
    d->m_submitForm->submitContentType = contentType;
    d->m_submitForm->submitBoundary = boundary;
  }
  else
    submitForm(u, args);
}


void Frame::parentCompleted()
{
    if (d->m_scheduledRedirection != noRedirectionScheduled && !d->m_redirectionTimer.isActive())
        startRedirectionTimer();
}

void Frame::childCompleted(bool complete)
{
    if (complete && !tree()->parent())
        d->m_bPendingChildRedirection = true;
    checkCompleted();
}

int Frame::zoomFactor() const
{
  return d->m_zoomFactor;
}

void Frame::setZoomFactor(int percent)
{  
  if (d->m_zoomFactor == percent)
      return;

  d->m_zoomFactor = percent;

  if (d->m_doc)
      d->m_doc->recalcStyle(NodeImpl::Force);

  for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
      child->setZoomFactor(d->m_zoomFactor);

  if (d->m_doc && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout())
      view()->layout();
}

void Frame::setJSStatusBarText(const String& text)
{
    d->m_kjsStatusBarText = text;
    setStatusBarText(d->m_kjsStatusBarText);
}

void Frame::setJSDefaultStatusBarText(const String& text)
{
    d->m_kjsDefaultStatusBarText = text;
    setStatusBarText(d->m_kjsDefaultStatusBarText);
}

String Frame::jsStatusBarText() const
{
    return d->m_kjsStatusBarText;
}

String Frame::jsDefaultStatusBarText() const
{
   return d->m_kjsDefaultStatusBarText;
}

QString Frame::referrer() const
{
   return d->m_referrer;
}

QString Frame::lastModified() const
{
  return d->m_lastModified;
}


void Frame::reparseConfiguration()
{
  setAutoloadImages( d->m_settings->autoLoadImages() );
  if (d->m_doc)
     d->m_doc->docLoader()->setShowAnimations( d->m_settings->showAnimations() );

  d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled(d->m_url.host());
  d->m_bJavaEnabled = d->m_settings->isJavaEnabled(d->m_url.host());
  d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled(d->m_url.host());

  QString userStyleSheet = d->m_settings->userStyleSheet();
  if ( !userStyleSheet.isEmpty() )
    setUserStyleSheet( KURL( userStyleSheet ) );
  else
    setUserStyleSheet( QString() );

  if(d->m_doc) d->m_doc->updateStyleSelector();
}

bool Frame::shouldDragAutoNode(NodeImpl *node, int x, int y) const
{
    // No KDE impl yet
    return false;
}

bool Frame::isPointInsideSelection(int x, int y)
{
    // Treat a collapsed selection like no selection.
    if (!d->m_selection.isRange())
        return false;
    if (!document()->renderer()) 
        return false;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    document()->renderer()->layer()->hitTest(nodeInfo, x, y);
    NodeImpl *innerNode = nodeInfo.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    Position pos(innerNode->renderer()->positionForCoordinates(x, y).deepEquivalent());
    if (pos.isNull())
        return false;

    NodeImpl *n = d->m_selection.start().node();
    while (n) {
        if (n == pos.node()) {
            if ((n == d->m_selection.start().node() && pos.offset() < d->m_selection.start().offset()) ||
                (n == d->m_selection.end().node() && pos.offset() > d->m_selection.end().offset())) {
                return false;
            }
            return true;
        }
        if (n == d->m_selection.end().node())
            break;
        n = n->traverseNextNode();
    }

   return false;
}

void Frame::selectClosestWordFromMouseEvent(MouseEvent *mouse, NodeImpl *innerNode, int x, int y)
{
    SelectionController selection;

    if (innerNode && innerNode->renderer() && mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        VisiblePosition pos(innerNode->renderer()->positionForCoordinates(x, y));
        if (pos.isNotNull()) {
            selection.moveTo(pos);
            selection.expandUsingGranularity(WordGranularity);
        }
    }
    
    if (selection.isRange()) {
        d->m_selectionGranularity = WordGranularity;
        d->m_beganSelectingText = true;
    }
    
    if (shouldChangeSelection(selection))
        setSelection(selection);
}

void Frame::handleMousePressEventDoubleClick(MouseEventWithHitTestResults* event)
{
    if (event->event()->button() == LeftButton) {
        if (selection().isRange())
            // A double-click when range is already selected
            // should not change the selection.  So, do not call
            // selectClosestWordFromMouseEvent, but do set
            // m_beganSelectingText to prevent khtmlMouseReleaseEvent
            // from setting caret selection.
            d->m_beganSelectingText = true;
        else {
            int x, y;
            view()->viewportToContents(event->event()->x(), event->event()->y(), x, y);
            selectClosestWordFromMouseEvent(event->event(), event->innerNode(), x, y);
        }
    }
}

void Frame::handleMousePressEventTripleClick(MouseEventWithHitTestResults* event)
{
    MouseEvent *mouse = event->event();
    NodeImpl *innerNode = event->innerNode();
    
    if (mouse->button() == LeftButton && innerNode && innerNode->renderer() &&
        mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        SelectionController selection;
        int x, y;
        view()->viewportToContents(event->event()->x(), event->event()->y(), x, y);
        VisiblePosition pos(innerNode->renderer()->positionForCoordinates(x, y));
        if (pos.isNotNull()) {
            selection.moveTo(pos);
            selection.expandUsingGranularity(ParagraphGranularity);
        }
        if (selection.isRange()) {
            d->m_selectionGranularity = ParagraphGranularity;
            d->m_beganSelectingText = true;
        }
        
        if (shouldChangeSelection(selection))
            setSelection(selection);
    }
}

void Frame::handleMousePressEventSingleClick(MouseEventWithHitTestResults* event)
{
    MouseEvent *mouse = event->event();
    NodeImpl *innerNode = event->innerNode();
    
    if (mouse->button() == LeftButton) {
        if (innerNode && innerNode->renderer() &&
            mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
            SelectionController sel;
            
            // Extend the selection if the Shift key is down, unless the click is in a link.
            bool extendSelection = mouse->shiftKey() && event->url().isNull();

            // Don't restart the selection when the mouse is pressed on an
            // existing selection so we can allow for text dragging.
            int x, y;
            view()->viewportToContents(event->event()->x(), event->event()->y(), x, y);
            if (!extendSelection && isPointInsideSelection(x, y))
                return;

            VisiblePosition visiblePos(innerNode->renderer()->positionForCoordinates(x, y));
            if (visiblePos.isNull())
                visiblePos = VisiblePosition(innerNode, innerNode->caretMinOffset(), DOWNSTREAM);
            Position pos = visiblePos.deepEquivalent();
            
            sel = selection();
            if (extendSelection && sel.isCaretOrRange()) {
                sel.clearModifyBias();
                
                // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
                // was created right-to-left
                Position start = sel.start();
                short before = RangeImpl::compareBoundaryPoints(pos.node(), pos.offset(), start.node(), start.offset());
                if (before <= 0) {
                    sel.setBaseAndExtent(pos.node(), pos.offset(), sel.end().node(), sel.end().offset());
                } else {
                    sel.setBaseAndExtent(start.node(), start.offset(), pos.node(), pos.offset());
                }

                if (d->m_selectionGranularity != CharacterGranularity) {
                    sel.expandUsingGranularity(d->m_selectionGranularity);
                }
                d->m_beganSelectingText = true;
            } else {
                sel = SelectionController(visiblePos);
                d->m_selectionGranularity = CharacterGranularity;
            }
            
            if (shouldChangeSelection(sel))
                setSelection(sel);
        }
    }
}

void Frame::khtmlMousePressEvent(MouseEventWithHitTestResults* event)
{
    DOMString url = event->url();
    MouseEvent *mouse = event->event();
    NodeImpl *innerNode = event->innerNode();

    d->m_mousePressNode = innerNode;
    d->m_dragStartPos = mouse->pos();

    if (mouse->button() == LeftButton || mouse->button() == MiddleButton) {
        d->m_bMousePressed = true;
        d->m_beganSelectingText = false;

        if (mouse->clickCount() == 2) {
            handleMousePressEventDoubleClick(event);
            return;
        }
        if (mouse->clickCount() >= 3) {
            handleMousePressEventTripleClick(event);
            return;
        }
        handleMousePressEventSingleClick(event);
    }
}

void Frame::handleMouseMoveEventSelection(MouseEventWithHitTestResults* event)
{
    // Mouse not pressed. Do nothing.
    if (!d->m_bMousePressed)
        return;

    MouseEvent *mouse = event->event();
    NodeImpl *innerNode = event->innerNode();

    if (mouse->button() != 0 || !innerNode || !innerNode->renderer() || !mouseDownMayStartSelect() || !innerNode->renderer()->shouldSelect())
        return;

    // handle making selection
    int x, y;
    view()->viewportToContents(event->event()->x(), event->event()->y(), x, y);
    VisiblePosition pos(innerNode->renderer()->positionForCoordinates(x, y));

    // Don't modify the selection if we're not on a node.
    if (pos.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in khtmlMousePressEvent, but not if the mouse press was on an existing selection.
    SelectionController sel = selection();
    sel.clearModifyBias();
    
    if (!d->m_beganSelectingText) {
        d->m_beganSelectingText = true;
        sel.moveTo(pos);
    }

    sel.setExtent(pos);
    if (d->m_selectionGranularity != CharacterGranularity)
        sel.expandUsingGranularity(d->m_selectionGranularity);

    if (shouldChangeSelection(sel))
        setSelection(sel);
}

void Frame::khtmlMouseMoveEvent(MouseEventWithHitTestResults* event)
{
    handleMouseMoveEventSelection(event);
}

void Frame::khtmlMouseReleaseEvent(MouseEventWithHitTestResults* event)
{
    stopAutoscrollTimer();
    
    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    d->m_bMousePressed = false;
  
    // Clear the selection if the mouse didn't move after the last mouse press.
    // We do this so when clicking on the selection, the selection goes away.
    // However, if we are editing, place the caret.
    if (mouseDownMayStartSelect() && !d->m_beganSelectingText
            && d->m_dragStartPos == event->event()->pos()
            && d->m_selection.isRange()) {
        SelectionController selection;
        NodeImpl *node = event->innerNode();
        if (node && node->isContentEditable() && node->renderer()) {
            int x, y;
            view()->viewportToContents(event->event()->x(), event->event()->y(), x, y);
            VisiblePosition pos = node->renderer()->positionForCoordinates(x, y);
            selection.moveTo(pos);
        }
        if (shouldChangeSelection(selection))
            setSelection(selection);
    }
    selectFrameElementInParentIfFullySelected();
}

void Frame::selectAll()
{
    if (!d->m_doc)
        return;
    
    NodeImpl *startNode = d->m_selection.start().node();
    NodeImpl *root = startNode && startNode->isContentEditable() ? startNode->rootEditableElement() : d->m_doc->documentElement();
    
    selectContentsOfNode(root);
    selectFrameElementInParentIfFullySelected();
}

bool Frame::selectContentsOfNode(NodeImpl* node)
{
    SelectionController sel = SelectionController(Position(node, 0), Position(node, maxDeepOffset(node)), DOWNSTREAM);    
    if (shouldChangeSelection(sel)) {
        setSelection(sel);
        return true;
    }
    return false;
}

bool Frame::shouldChangeSelection(const SelectionController &newselection) const
{
    return shouldChangeSelection(d->m_selection, newselection, newselection.affinity(), false);
}

bool Frame::shouldBeginEditing(const RangeImpl *range) const
{
    return true;
}

bool Frame::shouldEndEditing(const RangeImpl *range) const
{
    return true;
}

bool Frame::isContentEditable() const 
{
    if (!d->m_doc)
        return false;
    return d->m_doc->inDesignMode();
}

EditCommandPtr Frame::lastEditCommand()
{
    return d->m_lastEditCommand;
}

void dispatchKHTMLEditableContentChanged(NodeImpl* root)
{
    if (!root)
        return;
        
    ExceptionCode ec = 0;
    RefPtr<EventImpl> evt = new EventImpl(khtmlEditableContentChangedEvent, false, false);
    root->dispatchEvent(evt, ec, true);
}

void Frame::appliedEditing(EditCommandPtr &cmd)
{
    SelectionController sel(cmd.endingSelection());
    if (shouldChangeSelection(sel)) {
        setSelection(sel, false);
    }
    
    dispatchKHTMLEditableContentChanged(!selection().isNone() ? selection().start().node()->rootEditableElement() : 0);

    // Now set the typing style from the command. Clear it when done.
    // This helps make the case work where you completely delete a piece
    // of styled text and then type a character immediately after.
    // That new character needs to take on the style of the just-deleted text.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (cmd.typingStyle()) {
        setTypingStyle(cmd.typingStyle());
        cmd.setTypingStyle(0);
    }

    // Command will be equal to last edit command only in the case of typing
    if (d->m_lastEditCommand == cmd) {
        assert(cmd.isTypingCommand());
    }
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        registerCommandForUndo(cmd);
        d->m_lastEditCommand = cmd;
    }
    respondToChangedContents();
}

void Frame::unappliedEditing(EditCommandPtr &cmd)
{
    SelectionController sel(cmd.startingSelection());
    if (shouldChangeSelection(sel)) {
        setSelection(sel, true);
    }
    
    dispatchKHTMLEditableContentChanged(!selection().isNone() ? selection().start().node()->rootEditableElement() : 0);
        
    registerCommandForRedo(cmd);
    respondToChangedContents();
    d->m_lastEditCommand = EditCommandPtr::emptyCommand();
}

void Frame::reappliedEditing(EditCommandPtr &cmd)
{
    SelectionController sel(cmd.endingSelection());
    if (shouldChangeSelection(sel)) {
        setSelection(sel, true);
    }
    
    dispatchKHTMLEditableContentChanged(!selection().isNone() ? selection().start().node()->rootEditableElement() : 0);
        
    registerCommandForUndo(cmd);
    respondToChangedContents();
    d->m_lastEditCommand = EditCommandPtr::emptyCommand();
}

CSSMutableStyleDeclarationImpl *Frame::typingStyle() const
{
    return d->m_typingStyle.get();
}

void Frame::setTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    d->m_typingStyle = style;
}

void Frame::clearTypingStyle()
{
    d->m_typingStyle = 0;
}

JSValue* Frame::executeScript(const QString& filename, int baseLine, NodeImpl* n, const QString &script)
{
  // FIXME: This is missing stuff that the other executeScript has.
  // --> d->m_runningScripts and submitFormAgain.
  // Why is that OK?
  KJSProxyImpl *proxy = jScript();
  if (!proxy)
    return 0;
  JSValue* ret = proxy->evaluate(filename, baseLine, script, n);
  DocumentImpl::updateDocumentsRendering();
  return ret;
}

Frame *Frame::opener()
{
    return d->m_opener;
}

void Frame::setOpener(Frame* opener)
{
    if (d->m_opener)
        d->m_opener->d->m_openedFrames.remove(this);
    if (opener)
        opener->d->m_openedFrames.add(this);
    d->m_opener = opener;
}

bool Frame::openedByJS()
{
    return d->m_openedByJS;
}

void Frame::setOpenedByJS(bool _openedByJS)
{
    d->m_openedByJS = _openedByJS;
}

bool Frame::tabsToLinks() const
{
    return true;
}

bool Frame::tabsToAllControls() const
{
    return true;
}

void Frame::copyToPasteboard()
{
    issueCopyCommand();
}

void Frame::cutToPasteboard()
{
    issueCutCommand();
}

void Frame::pasteFromPasteboard()
{
    issuePasteCommand();
}

void Frame::pasteAndMatchStyle()
{
    issuePasteAndMatchStyleCommand();
}

void Frame::transpose()
{
    issueTransposeCommand();
}

void Frame::redo()
{
    issueRedoCommand();
}

void Frame::undo()
{
    issueUndoCommand();
}


void Frame::computeAndSetTypingStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    if (!style || style->length() == 0) {
        clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtr<CSSMutableStyleDeclarationImpl> mutableStyle = style->makeMutable();
    if (typingStyle()) {
        typingStyle()->merge(mutableStyle.get());
        mutableStyle = typingStyle();
    }

    NodeImpl *node = VisiblePosition(selection().start(), selection().affinity()).deepEquivalent().node();
    CSSComputedStyleDeclarationImpl computedStyle(node);
    computedStyle.diff(mutableStyle.get());
    
    // Handle block styles, substracting these from the typing style.
    RefPtr<CSSMutableStyleDeclarationImpl> blockStyle = mutableStyle->copyBlockProperties();
    blockStyle->diff(mutableStyle.get());
    if (document() && blockStyle->length() > 0) {
        EditCommandPtr cmd(new ApplyStyleCommand(document(), blockStyle.get(), 0, editingAction));
        cmd.apply();
    }
    
    // Set the remaining style as the typing style.
    d->m_typingStyle = mutableStyle.release();
}

void Frame::applyStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    switch (selection().state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET: {
            computeAndSetTypingStyle(style, editingAction);
            break;
        }
        case Selection::RANGE:
            if (document() && style) {
                EditCommandPtr cmd(new ApplyStyleCommand(document(), style, 0, editingAction));
                cmd.apply();
            }
            break;
    }
}

void Frame::applyParagraphStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    switch (selection().state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
        case Selection::RANGE:
            if (document() && style) {
                EditCommandPtr cmd(new ApplyStyleCommand(document(), style, 0, editingAction, ApplyStyleCommand::ForceBlockProperties));
                cmd.apply();
            }
            break;
    }
}

static void updateState(CSSMutableStyleDeclarationImpl *desiredStyle, CSSComputedStyleDeclarationImpl *computedStyle, bool &atStart, Frame::TriState &state)
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = desiredStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        DOMString desiredProperty = desiredStyle->getPropertyValue(propertyID);
        DOMString computedProperty = computedStyle->getPropertyValue(propertyID);
        Frame::TriState propertyState = equalIgnoringCase(desiredProperty, computedProperty)
            ? Frame::trueTriState : Frame::falseTriState;
        if (atStart) {
            state = propertyState;
            atStart = false;
        } else if (state != propertyState) {
            state = Frame::mixedTriState;
            break;
        }
    }
}

Frame::TriState Frame::selectionHasStyle(CSSStyleDeclarationImpl *style) const
{
    bool atStart = true;
    TriState state = falseTriState;

    RefPtr<CSSMutableStyleDeclarationImpl> mutableStyle = style->makeMutable();

    if (!d->m_selection.isRange()) {
        NodeImpl* nodeToRemove;
        RefPtr<CSSComputedStyleDeclarationImpl> selectionStyle = selectionComputedStyle(nodeToRemove);
        if (!selectionStyle)
            return falseTriState;
        updateState(mutableStyle.get(), selectionStyle.get(), atStart, state);
        if (nodeToRemove) {
            ExceptionCode ec = 0;
            nodeToRemove->remove(ec);
            assert(ec == 0);
        }
    } else {
        for (NodeImpl* node = d->m_selection.start().node(); node; node = node->traverseNextNode()) {
            RefPtr<CSSComputedStyleDeclarationImpl> computedStyle = new CSSComputedStyleDeclarationImpl(node);
            if (computedStyle)
                updateState(mutableStyle.get(), computedStyle.get(), atStart, state);
            if (state == mixedTriState)
                break;
            if (node == d->m_selection.end().node())
                break;
        }
    }

    return state;
}

bool Frame::selectionStartHasStyle(CSSStyleDeclarationImpl *style) const
{
    NodeImpl* nodeToRemove;
    RefPtr<CSSStyleDeclarationImpl> selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return false;

    RefPtr<CSSMutableStyleDeclarationImpl> mutableStyle = style->makeMutable();

    bool match = true;
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        if (!equalIgnoringCase(mutableStyle->getPropertyValue(propertyID), selectionStyle->getPropertyValue(propertyID))) {
            match = false;
            break;
        }
    }

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        assert(ec == 0);
    }

    return match;
}

DOMString Frame::selectionStartStylePropertyValue(int stylePropertyID) const
{
    NodeImpl *nodeToRemove;
    RefPtr<CSSStyleDeclarationImpl> selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return DOMString();

    DOMString value = selectionStyle->getPropertyValue(stylePropertyID);

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        assert(ec == 0);
    }

    return value;
}

CSSComputedStyleDeclarationImpl *Frame::selectionComputedStyle(NodeImpl *&nodeToRemove) const
{
    nodeToRemove = 0;

    if (!document())
        return 0;

    if (d->m_selection.isNone())
        return 0;

    RefPtr<RangeImpl> range(d->m_selection.toRange());
    Position pos = range->editingStartPosition();

    ElementImpl *elem = pos.element();
    if (!elem)
        return 0;
    
    RefPtr<ElementImpl> styleElement = elem;
    ExceptionCode ec = 0;

    if (d->m_typingStyle) {
        styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
        assert(ec == 0);

        styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), ec);
        assert(ec == 0);
        
        styleElement->appendChild(document()->createEditingTextNode(""), ec);
        assert(ec == 0);

        if (elem->renderer() && elem->renderer()->canHaveChildren()) {
            elem->appendChild(styleElement, ec);
        } else {
            NodeImpl *parent = elem->parent();
            NodeImpl *next = elem->nextSibling();

            if (next) {
                parent->insertBefore(styleElement, next, ec);
            } else {
                parent->appendChild(styleElement, ec);
            }
        }
        assert(ec == 0);

        nodeToRemove = styleElement.get();
    }

    return new CSSComputedStyleDeclarationImpl(styleElement);
}

void Frame::applyEditingStyleToBodyElement() const
{
    if (!d->m_doc)
        return;
        
    RefPtr<NodeListImpl> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        applyEditingStyleToElement(static_cast<ElementImpl *>(list->item(i)));    
    }
}

void Frame::removeEditingStyleFromBodyElement() const
{
    if (!d->m_doc)
        return;
        
    RefPtr<NodeListImpl> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        removeEditingStyleFromElement(static_cast<ElementImpl *>(list->item(i)));    
    }
}

void Frame::applyEditingStyleToElement(ElementImpl *element) const
{
    if (!element || !element->isHTMLElement())
        return;
    
    static_cast<HTMLElementImpl *>(element)->setContentEditable("true");
}

void Frame::removeEditingStyleFromElement(ElementImpl *element) const
{
    if (!element || !element->isHTMLElement())
        return;
        
    static_cast<HTMLElementImpl *>(element)->setContentEditable("false");        
}


bool Frame::isCharacterSmartReplaceExempt(const QChar &, bool)
{
    // no smart replace
    return true;
}

#if !NDEBUG
static HashSet<Frame*> lifeSupportSet;
#endif

void Frame::endAllLifeSupport()
{
#if !NDEBUG
    HashSet<Frame*> lifeSupportCopy = lifeSupportSet;
    HashSet<Frame*>::iterator end = lifeSupportCopy.end();
    for (HashSet<Frame*>::iterator it = lifeSupportCopy.begin(); it != end; ++it)
        (*it)->endLifeSupport();
#endif
}

void Frame::keepAlive()
{
    if (d->m_lifeSupportTimer.isActive())
        return;
    ref();
#if !NDEBUG
    lifeSupportSet.add(this);
#endif
    d->m_lifeSupportTimer.startOneShot(0);
}

void Frame::endLifeSupport()
{
    if (!d->m_lifeSupportTimer.isActive())
        return;
    d->m_lifeSupportTimer.stop();
#if !NDEBUG
    lifeSupportSet.remove(this);
#endif
    deref();
}

void Frame::lifeSupportTimerFired(Timer<Frame>*)
{
#if !NDEBUG
    lifeSupportSet.remove(this);
#endif
    deref();
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void Frame::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    Frame *parent = tree()->parent();
    if (!parent)
        return;
    FrameView *parentView = parent->view();
    if (!parentView)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!d->m_selection.isRange())
        return;
    if (!isStartOfDocument(VisiblePosition(d->m_selection.start(), d->m_selection.affinity())))
        return;
    if (!isEndOfDocument(VisiblePosition(d->m_selection.end(), d->m_selection.affinity())))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    DocumentImpl *doc = document();
    if (!doc)
        return;
    ElementImpl *ownerElement = doc->ownerElement();
    if (!ownerElement)
        return;
    NodeImpl *ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;
        
    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->isContentEditable())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex, SEL_DEFAULT_AFFINITY));
    VisiblePosition afterOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex + 1, VP_UPSTREAM_IF_POSSIBLE));

    // Focus on the parent frame, and then select from before this element to after.
    if (parent->shouldChangeSelection(SelectionController(beforeOwnerElement, afterOwnerElement))) {
        parentView->setFocus();
        parent->setSelection(SelectionController(beforeOwnerElement, afterOwnerElement));
    }
}

void Frame::handleFallbackContent()
{
    ElementImpl* owner = ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElementImpl *>(owner)->renderFallbackContent();
}

void Frame::setSettings(KHTMLSettings *settings)
{
    d->m_settings = settings;
}

void Frame::provisionalLoadStarted()
{
    // we don't want to wait until we get an actual http response back
    // to cancel pending redirects, otherwise they might fire before
    // that happens.
    cancelRedirection(true);
}

bool Frame::userGestureHint()
{
    Frame *rootFrame = this;
    while (rootFrame->tree()->parent())
        rootFrame = rootFrame->tree()->parent();

    if (rootFrame->jScript())
        return rootFrame->jScript()->interpreter()->wasRunByUserGesture();

    return true; // If JavaScript is disabled, a user gesture must have initiated the navigation
}

RenderObject *Frame::renderer() const
{
    DocumentImpl *doc = document();
    return doc ? doc->renderer() : 0;
}

ElementImpl* Frame::ownerElement()
{
    RenderPart* ownerElementRenderer = d->m_ownerRenderer;
    if (!ownerElementRenderer)
        return 0;
    return static_cast<ElementImpl*>(ownerElementRenderer->element());
}

RenderPart* Frame::ownerRenderer()
{
    return d->m_ownerRenderer;
}

IntRect Frame::selectionRect() const
{
    RenderCanvas *root = static_cast<RenderCanvas *>(renderer());
    if (!root)
        return IntRect();

    return root->selectionRect();
}

// returns FloatRect because going through IntRect would truncate any floats
FloatRect Frame::visibleSelectionRect() const
{
    if (!d->m_view)
        return FloatRect();
    
    return intersection(selectionRect(), d->m_view->visibleContentRect());
}

bool Frame::isFrameSet() const
{
    DocumentImpl* document = d->m_doc.get();
    if (!document || !document->isHTMLDocument())
        return false;
    NodeImpl *body = static_cast<HTMLDocumentImpl *>(document)->body();
    return body && body->renderer() && body->hasTagName(framesetTag);
}

bool Frame::openURL(const KURL &URL)
{
    ASSERT_NOT_REACHED();
    return true;
}

void Frame::didNotOpenURL(const KURL &URL)
{
    if (_submittedFormURL == URL) {
        _submittedFormURL = KURL();
    }
}

// Scans logically forward from "start", including any child frames
static HTMLFormElementImpl *scanForForm(NodeImpl *start)
{
    NodeImpl *n;
    for (n = start; n; n = n->traverseNextNode()) {
        if (n->hasTagName(formTag)) {
            return static_cast<HTMLFormElementImpl *>(n);
        } else if (n->isHTMLElement()
                   && static_cast<HTMLElementImpl *>(n)->isGenericFormElement()) {
            return static_cast<HTMLGenericFormElementImpl *>(n)->form();
        } else if (n->hasTagName(frameTag) || n->hasTagName(iframeTag)) {
            NodeImpl *childDoc = static_cast<HTMLFrameElementImpl *>(n)->contentDocument();
            HTMLFormElementImpl *frameResult = scanForForm(childDoc);
            if (frameResult) {
                return frameResult;
            }
        }
    }
    return 0;
}

// We look for either the form containing the current focus, or for one immediately after it
HTMLFormElementImpl *Frame::currentForm() const
{
    // start looking either at the active (first responder) node, or where the selection is
    NodeImpl *start = d->m_doc ? d->m_doc->focusNode() : 0;
    if (!start)
        start = selection().start().node();
    
    // try walking up the node tree to find a form element
    NodeImpl *n;
    for (n = start; n; n = n->parentNode()) {
        if (n->hasTagName(formTag))
            return static_cast<HTMLFormElementImpl *>(n);
        else if (n->isHTMLElement()
                   && static_cast<HTMLElementImpl *>(n)->isGenericFormElement())
            return static_cast<HTMLGenericFormElementImpl *>(n)->form();
    }
    
    // try walking forward in the node tree to find a form element
    return start ? scanForForm(start) : 0;
}

void Frame::setEncoding(const QString &name, bool userChosen)
{
    if (!d->m_workingURL.isEmpty())
        receivedFirstData();
    d->m_encoding = name;
    d->m_haveEncoding = userChosen;
}

void Frame::addData(const char *bytes, int length)
{
    ASSERT(d->m_workingURL.isEmpty());
    ASSERT(d->m_doc);
    ASSERT(d->m_doc->parsing());
    write(bytes, length);
}

// FIXME: should this go in SelectionController?
void Frame::revealSelection()
{
    IntRect rect;
    
    switch (selection().state()) {
        case Selection::NONE:
            return;
            
        case Selection::CARET:
            rect = selection().caretRect();
            break;
            
        case Selection::RANGE:
            rect = selectionRect();
            break;
    }
    
    Position start = selection().start();
    Position end = selection().end();
    ASSERT(start.node());
    if (start.node() && start.node()->renderer()) {
        RenderLayer *layer = start.node()->renderer()->enclosingLayer();
        if (layer) {
            ASSERT(!end.node() || !end.node()->renderer() 
                   || (end.node()->renderer()->enclosingLayer() == layer));
            layer->scrollRectToVisible(rect);
        }
    }
}

// FIXME: should this be here?
bool Frame::scrollOverflow(KWQScrollDirection direction, KWQScrollGranularity granularity)
{
    if (!document()) {
        return false;
    }
    
    NodeImpl *node = document()->focusNode();
    if (node == 0) {
        node = d->m_mousePressNode.get();
    }
    
    if (node != 0) {
        RenderObject *r = node->renderer();
        if (r != 0) {
            return r->scroll(direction, granularity);
        }
    }
    
    return false;
}

void Frame::handleAutoscroll(RenderLayer* layer)
{
    if (d->m_autoscrollTimer.isActive())
        return;
    d->m_autoscrollLayer = layer;
    startAutoscrollTimer();
}

void Frame::autoscrollTimerFired(Timer<Frame>*)
{
    bool isStillDown = MouseEvent::isMouseButtonDown(LeftButton);  
    if (!isStillDown){
        stopAutoscrollTimer();
        return;
    }
    if (d->m_autoscrollLayer) {
        d->m_autoscrollLayer->autoscroll();
    } 
}

void Frame::startAutoscrollTimer()
{
    d->m_autoscrollTimer.startRepeating(autoscrollInterval);
}

void Frame::stopAutoscrollTimer()
{
    d->m_autoscrollTimer.stop();
}

// FIXME: why is this here instead of on the FrameView?
void Frame::paint(GraphicsContext* p, const IntRect& rect)
{
#if !NDEBUG
    bool fillWithRed;
    if (p->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (!document() || document()->ownerElement())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (view() && view()->isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (_drawSelectionOnly)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (_elementToDraw)
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;
    
    if (fillWithRed)
        p->fillRect(rect, Color(0xFF, 0, 0));
#endif
    
    if (renderer()) {
        // _elementToDraw is used to draw only one element
        RenderObject *eltRenderer = _elementToDraw ? _elementToDraw->renderer() : 0;
        renderer()->layer()->paint(p, rect, _drawSelectionOnly, eltRenderer);

#if __APPLE__
        // Regions may have changed as a result of the visibility/z-index of element changing.
        if (renderer()->document()->dashboardRegionsDirty())
            renderer()->canvas()->view()->updateDashboardRegions();
#endif
    } else
        LOG_ERROR("called Frame::paint with nil renderer");
}

void Frame::adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float bottomLimit)
{
    RenderCanvas *root = static_cast<RenderCanvas *>(document()->renderer());
    if (root) {
        // Use a printer device, with painting disabled for the pagination phase
        GraphicsContext painter(true);
        painter.setPaintingDisabled(true);
        
        root->setTruncatedAt((int)floorf(oldBottom));
        IntRect dirtyRect(0, (int)floorf(oldTop),
                        root->docWidth(), (int)ceilf(oldBottom-oldTop));
        root->layer()->paint(&painter, dirtyRect);
        *newBottom = root->bestTruncatedAt();
        if (*newBottom == 0)
            *newBottom = oldBottom;
    } else
        *newBottom = oldBottom;
}

PausedTimeouts *Frame::pauseTimeouts()
{
#if SVG_SUPPORT
    if (d->m_doc && d->m_doc->svgExtensions())
        d->m_doc->accessSVGExtensions()->pauseAnimations();
#endif

    if (d->m_doc && d->m_jscript) {
        if (Window *w = Window::retrieveWindow(this))
            return w->pauseTimeouts();
    }
    return 0;
}

void Frame::resumeTimeouts(PausedTimeouts *t)
{
#if SVG_SUPPORT
    if (d->m_doc && d->m_doc->svgExtensions())
        d->m_doc->accessSVGExtensions()->unpauseAnimations();
#endif

    if (d->m_doc && d->m_jscript && d->m_bJScriptEnabled) {
        if (Window *w = Window::retrieveWindow(this))
            w->resumeTimeouts(t);
    }
}

bool Frame::canCachePage()
{
    // Only save page state if:
    // 1.  We're not a frame or frameset.
    // 2.  The page has no unload handler.
    // 3.  The page has no password fields.
    // 4.  The URL for the page is not https.
    // 5.  The page has no applets.
    if (tree()->childCount() || d->m_plugins.size() ||
        tree()->parent() ||
        d->m_url.protocol().startsWith("https") || 
        (d->m_doc && (d->m_doc->applets()->length() != 0 ||
                      d->m_doc->hasWindowEventListener(unloadEvent) ||
                      d->m_doc->hasPasswordField()))) {
        return false;
    }
    return true;
}

void Frame::saveWindowProperties(KJS::SavedProperties *windowProperties)
{
    Window *window = Window::retrieveWindow(this);
    if (window)
        window->saveProperties(*windowProperties);
}

void Frame::saveLocationProperties(SavedProperties *locationProperties)
{
    Window *window = Window::retrieveWindow(this);
    if (window) {
        JSLock lock;
        Location *location = window->location();
        location->saveProperties(*locationProperties);
    }
}

void Frame::restoreWindowProperties(SavedProperties *windowProperties)
{
    Window *window = Window::retrieveWindow(this);
    if (window)
        window->restoreProperties(*windowProperties);
}

void Frame::restoreLocationProperties(SavedProperties *locationProperties)
{
    Window *window = Window::retrieveWindow(this);
    if (window) {
        JSLock lock;
        Location *location = window->location();
        location->restoreProperties(*locationProperties);
    }
}

void Frame::saveInterpreterBuiltins(SavedBuiltins &interpreterBuiltins)
{
    if (jScript())
        jScript()->interpreter()->saveBuiltins(interpreterBuiltins);
}

void Frame::restoreInterpreterBuiltins(const SavedBuiltins &interpreterBuiltins)
{
    if (jScript())
        jScript()->interpreter()->restoreBuiltins(interpreterBuiltins);
}

Frame *Frame::frameForWidget(const Widget *widget)
{
    ASSERT_ARG(widget, widget);
    
    NodeImpl *node = nodeForWidget(widget);
    if (node)
        return frameForNode(node);
    
    // Assume all widgets are either form controls, or FrameViews.
    ASSERT(widget->isFrameView());
    return static_cast<const FrameView *>(widget)->frame();
}

Frame *Frame::frameForNode(NodeImpl *node)
{
    ASSERT_ARG(node, node);
    return node->getDocument()->frame();
}

NodeImpl* Frame::nodeForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);
    WidgetClient* client = widget->client();
    if (!client)
        return 0;
    return client->element(const_cast<Widget*>(widget));
}

void Frame::clearDocumentFocus(Widget *widget)
{
    NodeImpl *node = nodeForWidget(widget);
    ASSERT(node);
    node->getDocument()->setFocusNode(0);
}

QPtrList<Frame> &Frame::mutableInstances()
{
    static QPtrList<Frame> instancesList;
    return instancesList;
}

void Frame::updatePolicyBaseURL()
{
    if (tree()->parent() && tree()->parent()->document())
        setPolicyBaseURL(tree()->parent()->document()->policyBaseURL());
    else
        setPolicyBaseURL(d->m_url.url());
}

void Frame::setPolicyBaseURL(const DOMString &s)
{
    if (document())
        document()->setPolicyBaseURL(s);
    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setPolicyBaseURL(s);
}

void Frame::forceLayout()
{
    FrameView *v = d->m_view.get();
    if (v) {
        v->layout();
        // We cannot unschedule a pending relayout, since the force can be called with
        // a tiny rectangle from a drawRect update.  By unscheduling we in effect
        // "validate" and stop the necessary full repaint from occurring.  Basically any basic
        // append/remove DHTML is broken by this call.  For now, I have removed the optimization
        // until we have a better invalidation stategy. -dwh
        //v->unscheduleRelayout();
    }
}

void Frame::forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    RenderCanvas *root = static_cast<RenderCanvas *>(document()->renderer());
    if (root) {
        // This magic is basically copied from khtmlview::print
        int pageW = (int)ceilf(minPageWidth);
        root->setWidth(pageW);
        root->setNeedsLayoutAndMinMaxRecalc();
        forceLayout();
        
        // If we don't fit in the minimum page width, we'll lay out again. If we don't fit in the
        // maximum page width, we will lay out to the maximum page width and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        int rightmostPos = root->rightmostPosition();
        if (rightmostPos > minPageWidth) {
            pageW = kMin(rightmostPos, (int)ceilf(maxPageWidth));
            root->setWidth(pageW);
            root->setNeedsLayoutAndMinMaxRecalc();
            forceLayout();
        }
    }
}

void Frame::sendResizeEvent()
{
    if (DocumentImpl* doc = document())
        doc->dispatchWindowEvent(EventNames::resizeEvent, false, false);
}

void Frame::sendScrollEvent()
{
    FrameView *v = d->m_view.get();
    if (v) {
        DocumentImpl *doc = document();
        if (!doc)
            return;
        doc->dispatchHTMLEvent(scrollEvent, true, false);
    }
}

bool Frame::scrollbarsVisible()
{
    if (!view())
        return false;
    
    if (view()->hScrollBarMode() == ScrollBarAlwaysOff || view()->vScrollBarMode() == ScrollBarAlwaysOff)
        return false;
    
    return true;
}

void Frame::addMetaData(const QString &key, const QString &value)
{
    d->m_job->addMetaData(key, value);
}

// This does the same kind of work that Frame::openURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void Frame::scrollToAnchor(const KURL &URL)
{
    d->m_url = URL;
    started();
    
    gotoAnchor();
    
    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    d->m_bComplete = false;
    checkCompleted();
}

bool Frame::closeURL()
{
    saveDocumentState();
    stopLoading(true);
    clearUndoRedoOperations();
    return true;
}

bool Frame::canMouseDownStartSelect(NodeImpl* node)
{
    if (!node || !node->renderer())
        return true;
    
    // Check to see if khtml-user-select has been set to none
    if (!node->renderer()->canSelect())
        return false;
    
    // Some controls and images can't start a select on a mouse down.
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent()) {
        if (curr->style()->userSelect() == SELECT_IGNORE)
            return false;
    }
    
    return true;
}

void Frame::khtmlMouseDoubleClickEvent(MouseEventWithHitTestResults* event)
{
    passWidgetMouseDownEventToWidget(event, true);
}

bool Frame::passWidgetMouseDownEventToWidget(MouseEventWithHitTestResults* event, bool isDoubleClick)
{
    // Figure out which view to send the event to.
    RenderObject *target = event->innerNode() ? event->innerNode()->renderer() : 0;
    if (!target)
        return false;
    
    Widget* widget = RenderLayer::gScrollBar;
    if (!widget) {
        if (!target->isWidget())
            return false;
        widget = static_cast<RenderWidget *>(target)->widget();
    }
    
    // Doubleclick events don't exist in Cocoa.  Since passWidgetMouseDownEventToWidget will
    // just pass _currentEvent down to the widget,  we don't want to call it for events that
    // don't correspond to Cocoa events.  The mousedown/ups will have already been passed on as
    // part of the pressed/released handling.
    if (!isDoubleClick)
        return passMouseDownEventToWidget(widget);
    return true;
}

bool Frame::passWidgetMouseDownEventToWidget(RenderWidget *renderWidget)
{
    return passMouseDownEventToWidget(renderWidget->widget());
}

void Frame::clearTimers(FrameView *view)
{
    if (view) {
        view->unscheduleRelayout();
        if (view->frame()) {
            DocumentImpl* document = view->frame()->document();
            if (document && document->renderer() && document->renderer()->layer())
                document->renderer()->layer()->suspendMarquees();
        }
    }
}

void Frame::clearTimers()
{
    clearTimers(d->m_view.get());
}

// FIXME: selection controller?
void Frame::centerSelectionInVisibleArea() const
{
    IntRect rect;
    
    switch (selection().state()) {
        case Selection::NONE:
            return;
            
        case Selection::CARET:
            rect = selection().caretRect();
            break;
            
        case Selection::RANGE:
            rect = selectionRect();
            break;
    }
    
    Position start = selection().start();
    Position end = selection().end();
    ASSERT(start.node());
    if (start.node() && start.node()->renderer()) {
        RenderLayer *layer = start.node()->renderer()->enclosingLayer();
        if (layer) {
            ASSERT(!end.node() || !end.node()->renderer() 
                   || (end.node()->renderer()->enclosingLayer() == layer));
            layer->scrollRectToVisible(rect, RenderLayer::gAlignCenterAlways, RenderLayer::gAlignCenterAlways);
        }
    }
}

RenderStyle *Frame::styleForSelectionStart(NodeImpl *&nodeToRemove) const
{
    nodeToRemove = 0;
    
    if (!document())
        return 0;
    if (d->m_selection.isNone())
        return 0;
    
    Position pos = VisiblePosition(d->m_selection.start(), d->m_selection.affinity()).deepEquivalent();
    if (!pos.inRenderedContent())
        return 0;
    NodeImpl *node = pos.node();
    if (!node)
        return 0;
    
    if (!d->m_typingStyle)
        return node->renderer()->style();
    
    ExceptionCode ec = 0;
    RefPtr<ElementImpl> styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
    ASSERT(ec == 0);
    
    styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), ec);
    ASSERT(ec == 0);
    
    styleElement->appendChild(document()->createEditingTextNode(""), ec);
    ASSERT(ec == 0);
    
    node->parentNode()->appendChild(styleElement, ec);
    ASSERT(ec == 0);
    
    nodeToRemove = styleElement.get();    
    return styleElement->renderer()->style();
}

void Frame::setMediaType(const QString &type)
{
    if (d->m_view)
        d->m_view->setMediaType(type);
}

void Frame::setSelectionFromNone()
{
    // Put the caret someplace if the selection is empty and the part is editable.
    // This has the effect of flashing the caret in a contentEditable view automatically 
    // without requiring the programmer to set a selection explicitly.
    DocumentImpl *doc = document();
    if (doc && selection().isNone() && isContentEditable()) {
        NodeImpl *node = doc->documentElement();
        while (node) {
            // Look for a block flow, but skip over the HTML element, since we really
            // want to get at least as far as the the BODY element in a document.
            if (node->isBlockFlow() && node->hasTagName(htmlTag))
                break;
            node = node->traverseNextNode();
        }
        if (node)
            setSelection(SelectionController(Position(node, 0), DOWNSTREAM));
    }
}

bool Frame::displaysWithFocusAttributes() const
{
    return d->m_isFocused;
}

void Frame::setDisplaysWithFocusAttributes(bool flag)
{
    if (d->m_isFocused == flag)
        return;
    
    d->m_isFocused = flag;

    // This method does the job of updating the view based on whether the view is "active".
    // This involves three kinds of drawing updates:

    // 1. The background color used to draw behind selected content (active | inactive color)
    if (d->m_view)
        d->m_view->updateContents(enclosingIntRect(visibleSelectionRect()));

    // 2. Caret blinking (blinks | does not blink)
    if (flag)
        setSelectionFromNone();
    setCaretVisible(flag);
    
    // 3. The drawing of a focus ring around links in web pages.
    DocumentImpl *doc = document();
    if (doc) {
        NodeImpl *node = doc->focusNode();
        if (node) {
            node->setChanged();
            if (node->renderer() && node->renderer()->style()->hasAppearance())
                theme()->stateChanged(node->renderer(), FocusState);
        }
    }
}

void Frame::setWindowHasFocus(bool flag)
{
    if (m_windowHasFocus == flag)
        return;
    m_windowHasFocus = flag;
    
    if (DocumentImpl *doc = document())
        doc->dispatchWindowEvent(flag ? focusEvent : blurEvent, false, false);
}

QChar Frame::backslashAsCurrencySymbol() const
{
    DocumentImpl *doc = document();
    if (!doc)
        return '\\';
    Decoder *decoder = doc->decoder();
    if (!decoder)
        return '\\';

    return decoder->encoding().backslashAsCurrencySymbol();
}

bool Frame::markedTextUsesUnderlines() const
{
    return m_markedTextUsesUnderlines;
}

QValueList<MarkedTextUnderline> Frame::markedTextUnderlines() const
{
    return m_markedTextUnderlines;
}

unsigned Frame::highlightAllMatchesForString(const QString &target, bool caseFlag)
{
    if (target.isEmpty()) {
        return 0;
    }
    
    RefPtr<RangeImpl> searchRange(rangeOfContents(document()));
    
    int exception = 0;
    unsigned matchCount = 0;
    do {
        RefPtr<RangeImpl> resultRange(findPlainText(searchRange.get(), target, true, caseFlag));
        if (resultRange->collapsed(exception))
            break;
        
        ++matchCount;
        document()->addMarker(resultRange.get(), DocumentMarker::TextMatch);
        
        setStart(searchRange.get(), endVisiblePosition(resultRange.get(), DOWNSTREAM));
    } while (true);
    
    return matchCount;
}

void Frame::prepareForUserAction()
{
    // Reset the multiple form submission protection code.
    // We'll let you submit the same form twice if you do two separate user actions.
    _submittedFormURL = KURL();
}

NodeImpl *Frame::mousePressNode()
{
    return d->m_mousePressNode.get();
}

bool Frame::isComplete() const
{
    return d->m_bComplete;
}

bool Frame::isLoadingMainResource() const
{
    return d->m_bLoadingMainResource;
}

FrameTree *Frame::tree() const
{
    return &d->m_treeNode;
}

void Frame::detachFromView()
{
}

KURL Frame::url() const
{
    return d->m_url;
}

void Frame::startRedirectionTimer()
{
    d->m_redirectionTimer.startOneShot(d->m_delayRedirect);
}

void Frame::stopRedirectionTimer()
{
    d->m_redirectionTimer.stop();
}

void Frame::frameDetached()
{
}

void Frame::updateBaseURLForEmptyDocument()
{
    ElementImpl* owner = ownerElement();
    // FIXME: Should embed be included?
    if (owner && (owner->hasTagName(iframeTag) || owner->hasTagName(objectTag) || owner->hasTagName(embedTag)))
        d->m_doc->setBaseURL(tree()->parent()->d->m_doc->baseURL());
}

Page* Frame::page() const
{
    return d->m_page;
}

void Frame::completed(bool complete)
{
    ref();
    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->parentCompleted();
    if (Frame* parent = tree()->parent())
        parent->childCompleted(complete);
    submitFormAgain();
    deref();
}

void Frame::setStatusBarText(const String&)
{
}

void Frame::started()
{
    for (Frame* frame = this; frame; frame = frame->tree()->parent())
        frame->d->m_bComplete = false;
}

void Frame::disconnectOwnerRenderer()
{
    d->m_ownerRenderer = 0;
}

} // namespace WebCore
