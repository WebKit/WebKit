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
 
#include <KWQKHTMLPart.h>

 
KHTMLPart::KHTMLPart()
{
}


KHTMLPart::KHTMLPart(const KURL &url )
{
}


KHTMLPart::~KHTMLPart()
{
}


bool KHTMLPart::openURL( const KURL &url )
{
}

bool KHTMLPart::closeURL()
{
}


DOM::HTMLDocument KHTMLPart::htmlDocument() const
{
}


DOM::Document KHTMLPart::document() const
{
}


void KHTMLPart::setJScriptEnabled( bool enable )
{
}


bool KHTMLPart::jScriptEnabled() const
{
}


void KHTMLPart::enableMetaRefresh( bool enable )
{
}


bool KHTMLPart::metaRefreshEnabled() const
{
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const QString &script )
{
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const DOM::Node &n, const QString &script )
{
}


void KHTMLPart::setJavaEnabled( bool enable )
{
}


bool KHTMLPart::javaEnabled() const
{
}


KJavaAppletContext *KHTMLPart::javaContext()
{
}


KJavaAppletContext *KHTMLPart::createJavaContext()
{
}


void KHTMLPart::setPluginsEnabled( bool enable )
{
}


bool KHTMLPart::pluginsEnabled() const
{
}


void KHTMLPart::setAutoloadImages( bool enable )
{
}


bool KHTMLPart::autoloadImages() const
{
}


void KHTMLPart::setOnlyLocalReferences(bool enable)
{
}


bool KHTMLPart::onlyLocalReferences() const
{
}


void KHTMLPart::begin( const KURL &url, int xOffset, int yOffset)
{
}


void KHTMLPart::write( const char *str, int len)
{
}


void KHTMLPart::write( const QString &str )
{
}


void KHTMLPart::end()
{
}


void KHTMLPart::setBaseURL( const KURL &url )
{
}


KURL KHTMLPart::baseURL() const
{
}


void KHTMLPart::setBaseTarget( const QString &target )
{
}


QString KHTMLPart::baseTarget() const
{
}


bool KHTMLPart::setCharset( const QString &name, bool override = false )
{
}


bool KHTMLPart::setEncoding( const QString &name, bool override = false )
{
}



QString KHTMLPart::encoding()
{
}


void KHTMLPart::setUserStyleSheet(const KURL &url)
{
}


void KHTMLPart::setUserStyleSheet(const QString &styleSheet)
{
}


void KHTMLPart::setFontSizes(const QValueList<int> &newFontSizes )
{
}


QValueList<int> KHTMLPart::fontSizes() const
{
}


void KHTMLPart::resetFontSizes()
{
}


void KHTMLPart::setStandardFont( const QString &name )
{
}


void KHTMLPart::setFixedFont( const QString &name )
{
}


// DUBIOUS, this should be handled by the view, also isn't the anchor a node?
bool KHTMLPart::gotoAnchor( const QString &name )
{
}


void KHTMLPart::setURLCursor( const QCursor &c )
{
}


const QCursor& KHTMLPart::urlCursor() const
{
}


// DUBIOUS, perhaps searching should be handled externally
void KHTMLPart::findTextBegin()
{
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QRegExp &exp, bool forward )
{
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QString &str, bool forward, bool caseSensitive )
{
}


// DUBIOUS, perhaps selection should be managed externally
QString KHTMLPart::selectedText() const
{
}


// DUBIOUS, perhaps selection should be managed externally
DOM::Range KHTMLPart::selection() const
{
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::setSelection( const DOM::Range & )
{
}


// DUBIOUS, perhaps selection should be managed externally
bool KHTMLPart::hasSelection() const
{
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::selectAll()
{
}


void KHTMLPart::setJSStatusBarText( const QString &text )
{
}


void KHTMLPart::setJSDefaultStatusBarText( const QString &text )
{
}


QString KHTMLPart::jsStatusBarText() const
{
}


QString KHTMLPart::jsDefaultStatusBarText() const
{
}


DOM::HTMLDocumentImpl *KHTMLPart::docImpl() const
{
}


DOM::DocumentImpl *KHTMLPart::xmlDocImpl() const
{
}


const KHTMLSettings *KHTMLPart::settings() const
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KJSProxy *KHTMLPart::jScript()
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KURL KHTMLPart::completeURL( const QString &url, const QString &target = QString::null )
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


const KURL & KHTMLPart::url() const
{
     NSLog (@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::scheduleRedirection( int delay, const QString &url )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KHTMLView *KHTMLPart::view() const
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QWidget *KHTMLPart::widget()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KHTMLPart *KHTMLPart::opener()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KHTMLPart *KHTMLPart::parentPart()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


const QList<KParts::ReadOnlyPart> KHTMLPart::frames() const
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KHTMLPart *KHTMLPart::findFrame( const QString &f )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::setOpener(KHTMLPart *_opener)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool KHTMLPart::openedByJS()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::setOpenedByJS(bool _openedByJS)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


KParts::BrowserExtension *KHTMLPart::browserExtension() const
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


DOM::EventListener *KHTMLPart::createHTMLEventListener( QString code )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QString KHTMLPart::requestFrameName()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool KHTMLPart::frameExists( const QString &frameName )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool KHTMLPart::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                    const QStringList &args, bool isIFrame)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::emitUnloadEvent()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::submitForm( const char *action, const QString &url, const QByteArray &formData,
                        const QString &target, const QString& contentType,
                        const QString& boundary )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::urlSelected( const QString &url, int button = 0, int state = 0,
                        const QString &_target = QString::null )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                    const QStringList &args)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::nodeActivated(const DOM::Node &)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QVariant KHTMLPart::executeScheduledScript()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::stopAutoScroll()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void KHTMLPart::overURL( const QString &url, const QString &target )
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}



