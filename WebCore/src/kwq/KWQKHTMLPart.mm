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

