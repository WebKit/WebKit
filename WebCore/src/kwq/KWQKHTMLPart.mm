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
#include <kwqdebug.h>
 
#include <KWQKHTMLPart.h>


KHTMLPart::KHTMLPart()
{
    _logNotYetImplemented();
}


KHTMLPart::KHTMLPart(const KURL &url )
{
    _logNotYetImplemented();
}


KHTMLPart::~KHTMLPart()
{
    _logNotYetImplemented();
}


bool KHTMLPart::openURL( const KURL &url )
{
    _logNotYetImplemented();
}

bool KHTMLPart::closeURL()
{
    _logNotYetImplemented();
}


DOM::HTMLDocument KHTMLPart::htmlDocument() const
{
    _logNotYetImplemented();
}


DOM::Document KHTMLPart::document() const
{
    _logNotYetImplemented();
}


void KHTMLPart::setJScriptEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::jScriptEnabled() const
{
    _logNotYetImplemented();
}


void KHTMLPart::enableMetaRefresh( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::metaRefreshEnabled() const
{
    _logNotYetImplemented();
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const QString &script )
{
    _logNotYetImplemented();
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const DOM::Node &n, const QString &script )
{
    _logNotYetImplemented();
}


void KHTMLPart::setJavaEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::javaEnabled() const
{
    _logNotYetImplemented();
}


KJavaAppletContext *KHTMLPart::javaContext()
{
    _logNotYetImplemented();
}


KJavaAppletContext *KHTMLPart::createJavaContext()
{
    _logNotYetImplemented();
}


void KHTMLPart::setPluginsEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::pluginsEnabled() const
{
    _logNeverImplemented();
}


void KHTMLPart::setAutoloadImages( bool enable )
{
    _logNeverImplemented();
}


bool KHTMLPart::autoloadImages() const
{
    _logNeverImplemented();
}


void KHTMLPart::setOnlyLocalReferences(bool enable)
{
    _logNeverImplemented();
}


bool KHTMLPart::onlyLocalReferences() const
{
    _logNeverImplemented();
}


void KHTMLPart::begin( const KURL &url, int xOffset, int yOffset)
{
    _logNeverImplemented();
}


void KHTMLPart::write( const char *str, int len)
{
    _logNeverImplemented();
}


void KHTMLPart::write( const QString &str )
{
    _logNeverImplemented();
}


void KHTMLPart::end()
{
    _logNeverImplemented();
}


void KHTMLPart::setBaseURL( const KURL &url )
{
    _logNeverImplemented();
}


KURL KHTMLPart::baseURL() const
{
    _logNeverImplemented();
}


void KHTMLPart::setBaseTarget( const QString &target )
{
    _logNeverImplemented();
}


QString KHTMLPart::baseTarget() const
{
    _logNeverImplemented();
}


bool KHTMLPart::setCharset( const QString &name, bool override = false )
{
    _logNeverImplemented();
}


bool KHTMLPart::setEncoding( const QString &name, bool override = false )
{
    _logNeverImplemented();
}



QString KHTMLPart::encoding()
{
    _logNeverImplemented();
}


void KHTMLPart::setUserStyleSheet(const KURL &url)
{
    _logNeverImplemented();
}


void KHTMLPart::setUserStyleSheet(const QString &styleSheet)
{
    _logNeverImplemented();
}


void KHTMLPart::setFontSizes(const QValueList<int> &newFontSizes )
{
    _logNeverImplemented();
}


QValueList<int> KHTMLPart::fontSizes() const
{
    _logNeverImplemented();
}


void KHTMLPart::resetFontSizes()
{
    _logNeverImplemented();
}


void KHTMLPart::setStandardFont( const QString &name )
{
    _logNeverImplemented();
}


void KHTMLPart::setFixedFont( const QString &name )
{
    _logNeverImplemented();
}


// DUBIOUS, this should be handled by the view, also isn't the anchor a node?
bool KHTMLPart::gotoAnchor( const QString &name )
{
    _logNeverImplemented();
}


void KHTMLPart::setURLCursor( const QCursor &c )
{
    _logNeverImplemented();
}


const QCursor& KHTMLPart::urlCursor() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
void KHTMLPart::findTextBegin()
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QRegExp &exp, bool forward )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QString &str, bool forward, bool caseSensitive )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
QString KHTMLPart::selectedText() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
DOM::Range KHTMLPart::selection() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::setSelection( const DOM::Range & )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
bool KHTMLPart::hasSelection() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::selectAll()
{
    _logNeverImplemented();
}


void KHTMLPart::setJSStatusBarText( const QString &text )
{
    _logNeverImplemented();
}


void KHTMLPart::setJSDefaultStatusBarText( const QString &text )
{
    _logNeverImplemented();
}


QString KHTMLPart::jsStatusBarText() const
{
    _logNeverImplemented();
}


QString KHTMLPart::jsDefaultStatusBarText() const
{
    _logNeverImplemented();
}


DOM::HTMLDocumentImpl *KHTMLPart::docImpl() const
{
    _logNeverImplemented();
}


DOM::DocumentImpl *KHTMLPart::xmlDocImpl() const
{
    _logNeverImplemented();
}


const KHTMLSettings *KHTMLPart::settings() const
{
    _logNotYetImplemented();
}


KJSProxy *KHTMLPart::jScript()
{
    _logNotYetImplemented();
}


KURL KHTMLPart::completeURL( const QString &url, const QString &target = QString::null )
{
    _logNotYetImplemented();
}


const KURL & KHTMLPart::url() const
{
    _logNotYetImplemented();
}


void KHTMLPart::scheduleRedirection( int delay, const QString &url )
{
    _logNeverImplemented();
}


KHTMLView *KHTMLPart::view() const
{
    _logNotYetImplemented();
}


QWidget *KHTMLPart::widget()
{
    _logNotYetImplemented();
}


KHTMLPart *KHTMLPart::opener()
{
    _logNeverImplemented();
}


KHTMLPart *KHTMLPart::parentPart()
{
    _logNeverImplemented();
}


const QList<KParts::ReadOnlyPart> KHTMLPart::frames() const
{
    _logNeverImplemented();
}


KHTMLPart *KHTMLPart::findFrame( const QString &f )
{
    _logNeverImplemented();
}


void KHTMLPart::setOpener(KHTMLPart *_opener)
{
    _logNeverImplemented();
}


bool KHTMLPart::openedByJS()
{
    _logNeverImplemented();
}


void KHTMLPart::setOpenedByJS(bool _openedByJS)
{
    _logNeverImplemented();
}


KParts::BrowserExtension *KHTMLPart::browserExtension() const
{
    _logNeverImplemented();
}


DOM::EventListener *KHTMLPart::createHTMLEventListener( QString code )
{
    _logNeverImplemented();
}


QString KHTMLPart::requestFrameName()
{
    _logNeverImplemented();
}


bool KHTMLPart::frameExists( const QString &frameName )
{
    _logNeverImplemented();
}


bool KHTMLPart::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                    const QStringList &args, bool isIFrame)
{
    _logNeverImplemented();
}


void KHTMLPart::emitUnloadEvent()
{
    _logNeverImplemented();
}


void KHTMLPart::submitForm( const char *action, const QString &url, const QByteArray &formData,
                        const QString &target, const QString& contentType,
                        const QString& boundary )
{
    _logNeverImplemented();
}


void KHTMLPart::urlSelected( const QString &url, int button = 0, int state = 0,
                        const QString &_target = QString::null )
{
    _logNeverImplemented();
}


bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                    const QStringList &args)
{
    _logNeverImplemented();
}


void KHTMLPart::nodeActivated(const DOM::Node &)
{
    _logNeverImplemented();
}


QVariant KHTMLPart::executeScheduledScript()
{
    _logNeverImplemented();
}


void KHTMLPart::stopAutoScroll()
{
    _logNeverImplemented();
}


void KHTMLPart::overURL( const QString &url, const QString &target )
{
    _logNeverImplemented();
}



