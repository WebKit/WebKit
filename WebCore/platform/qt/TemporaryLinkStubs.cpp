/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
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
#include "CachedResource.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "Font.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GlobalHistory.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlugInInfoStore.h"
#include "RenderTheme.h"
#include "SystemTime.h"
#include "TextBoundaries.h"
#include "Widget.h"
#include <stdio.h>
#include <stdlib.h>

using namespace WebCore;

FloatRect Font::selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int, int, int) const { notImplemented(); return FloatRect(); }
int Font::offsetForPositionForComplexText(const TextRun&, const TextStyle&, int, bool) const { notImplemented(); return 0; }

bool WebCore::historyContains(DeprecatedString const&) { return false; }

// LocalizedStrings
String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { return String(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::defaultLanguage() { return "en"; }
String WebCore::searchableIndexIntroduction() { return "Searchable Index"; }
String WebCore::fileButtonChooseFileLabel() { return "Choose File"; }
String WebCore::fileButtonNoFileSelectedLabel() { return "No file selected"; }
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadImageToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyImageToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { return String(); }
String WebCore::contextMenuItemTagCopy() { return String(); }
String WebCore::contextMenuItemTagGoBack() { return String(); }
String WebCore::contextMenuItemTagGoForward() { return String(); }
String WebCore::contextMenuItemTagStop() { return String(); }
String WebCore::contextMenuItemTagReload() { return String(); }
String WebCore::contextMenuItemTagCut() { return String(); }
String WebCore::contextMenuItemTagPaste() { return String(); }
String WebCore::contextMenuItemTagNoGuessesFound() { return String(); }
String WebCore::contextMenuItemTagIgnoreSpelling() { return String(); }
String WebCore::contextMenuItemTagLearnSpelling() { return String(); }
String WebCore::contextMenuItemTagSearchWeb() { return String(); }
String WebCore::contextMenuItemTagLookUpInDictionary() { return String(); }
String WebCore::contextMenuItemTagOpenLink() { return String(); }
String WebCore::contextMenuItemTagIgnoreGrammar() { return String(); }
String WebCore::contextMenuItemTagSpellingMenu() { return String(); }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show) { return String(); }
String WebCore::contextMenuItemTagCheckSpelling() { return String(); }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { return String(); }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { return String(); }
String WebCore::contextMenuItemTagFontMenu() { return String(); }
String WebCore::contextMenuItemTagBold() { return String(); }
String WebCore::contextMenuItemTagItalic() { return String(); }
String WebCore::contextMenuItemTagUnderline() { return String(); }
String WebCore::contextMenuItemTagOutline() { return String(); }
String WebCore::contextMenuItemTagWritingDirectionMenu() { return String(); }
String WebCore::contextMenuItemTagDefaultDirection() { return String(); }
String WebCore::contextMenuItemTagLeftToRight() { return String(); }
String WebCore::contextMenuItemTagRightToLeft() { return String(); }
String WebCore::searchMenuNoRecentSearchesText() { return String(); }
String WebCore::searchMenuRecentSearchesText() { return String(); }
String WebCore::searchMenuClearRecentSearchesText() { return String(); }
String WebCore::AXWebAreaText() { return String(); }
String WebCore::AXLinkText() { return String(); }
String WebCore::AXListMarkerText() { return String(); }
String WebCore::AXImageMapText() { return String(); }
String WebCore::AXHeadingText() { return String(); }

void Frame::setNeedsReapplyStyles() { notImplemented(); }

void FrameView::updateBorder() { notImplemented(); }

bool AXObjectCache::gAccessibilityEnabled = false;

Vector<char> loadResourceIntoArray(const char*) { return Vector<char>(); }

namespace WebCore {

Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }

float userIdleTime() { notImplemented(); return 0.0; }

}

// vim: ts=4 sw=4 et
