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

#include <browserextension.h>

namespace KParts {

// class BrowserExtension ======================================================

BrowserExtension::~BrowserExtension()
{
    _logNotYetImplemented();
}

static BrowserInterface *tempBrowserInterface = 0;

BrowserInterface *BrowserExtension::browserInterface() const
{
	_logNotYetImplemented();
    return tempBrowserInterface;
}


void BrowserExtension::createNewWindow(const KURL &)
{
    _logNotYetImplemented();
}

void BrowserExtension::createNewWindow(const KURL &, const KParts::URLArgs &, 
    const KParts::WindowArgs &, KParts::ReadOnlyPart *&)
{
    _logNotYetImplemented();
}

void BrowserExtension::openURLRequest(const KURL &, const KParts::URLArgs &)
{
    _logNotYetImplemented();
}

// class BrowserHostExtension ==================================================

BrowserHostExtension::BrowserHostExtension()
{
    _logNotYetImplemented();
}


BrowserHostExtension::~BrowserHostExtension()
{
    _logNotYetImplemented();
}

// struct URLArgs ==============================================================

URLArgs::URLArgs()
{
    _logNotYetImplemented();
}

URLArgs::URLArgs( const URLArgs &)
{
    _logNotYetImplemented();
}

URLArgs::~URLArgs()
{
    _logNotYetImplemented();
}

// struct WindowArgs ===========================================================

WindowArgs::WindowArgs()
{
    _logNotYetImplemented();
}

WindowArgs::WindowArgs(const WindowArgs &)
{
    _logNotYetImplemented();
}


} // namespace KParts



