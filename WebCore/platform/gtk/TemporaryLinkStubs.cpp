/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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

#include "config.h"

#include "AXObjectCache.h"
#include "CachedPage.h"
#include "Clipboard.h"
#include "Editor.h"
#include "FrameView.h"
#include "FTPDirectoryDocument.h"
#include "GlobalHistory.h"
#include "Language.h"
#include "NotImplemented.h"
#include "PlugInInfoStore.h"
#include "TextBoundaries.h"
#include "TextBreakIteratorInternalICU.h"

using namespace WebCore;

// This function loads resources from WebKit
// This does not belong here and I'm not sure where
// it should go
// I don't know what the plans or design is
// for none code resources
Vector<char> loadResourceIntoArray(const char* resourceName)
{
    Vector<char> resource;
    //if (strcmp(resourceName,"missingImage") == 0) {
    //}
    return resource;
}

namespace WebCore {
    class Page;
}

void FrameView::updateBorder() { notImplemented(); }

int WebCore::findNextWordFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
void WebCore::findWordBoundary(UChar const* str, int len, int position, int* start, int* end) {*start = position; *end = position; }
const char* WebCore::currentTextBreakLocaleID() { notImplemented(); return "en_us"; }


/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
bool AXObjectCache::gAccessibilityEnabled = false;

bool WebCore::historyContains(DeprecatedString const&) { return false; }

String WebCore::defaultLanguage() { return "en"; }

PluginInfo* PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
bool WebCore::PlugInInfoStore::supportsMIMEType(const WebCore::String&) { notImplemented(); return false; }
void WebCore::refreshPlugins(bool) { notImplemented(); }


Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void CachedPage::close() { notImplemented(); }

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy) { notImplemented(); return 0; }

namespace WebCore {
Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
float userIdleTime() { notImplemented(); return 0.0; }
void callOnMainThread(void (*)()) { notImplemented(); }
}

