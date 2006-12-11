/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "FrameWin.h"

#include "TextResourceDecoder.h"
#include "Document.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "FrameLoaderClientWin.h"
#include "FrameLoadRequest.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "Settings.h"
#include "PlatformKeyboardEvent.h"
#include "Plugin.h"
#include "RenderFrame.h"
#include "ResourceHandle.h"
#include <windows.h>

namespace WebCore {

FrameWin::FrameWin(Page* page, HTMLFrameOwnerElement* ownerElement, FrameWinClient* client)
    : Frame(page, ownerElement, new FrameLoaderClientWin())
    , m_client(client)
{
    Settings* settings = new Settings();
    settings->setAutoLoadImages(true);
    settings->setMediumFixedFontSize(13);
    settings->setMediumFontSize(16);
    settings->setSerifFontName("Times New Roman");
    settings->setFixedFontName("Courier New");
    settings->setSansSerifFontName("Arial");
    settings->setStdFontName("Times New Roman");
    settings->setIsJavaScriptEnabled(true);
    setSettings(settings);
}

FrameWin::~FrameWin()
{
    setView(0);
    loader()->clearRecordedFormValues();    
}

void FrameWin::runJavaScriptAlert(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    UChar nullChar = 0;
    text += String(&nullChar, 1);
    MessageBox(view()->containingWindow(), text.characters(), L"JavaScript Alert", MB_OK);
}

bool FrameWin::runJavaScriptConfirm(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    UChar nullChar = 0;
    text += String(&nullChar, 1);
    return MessageBox(view()->containingWindow(), text.characters(), L"JavaScript Alert", MB_OKCANCEL) == IDOK;
}

// FIXME: This needs to be unified with the keyPress method on FrameMac
bool FrameWin::keyPress(const PlatformKeyboardEvent& keyEvent)
{
    bool result;
    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document *doc = document();
    if (!doc)
        return false;
    Node *node = doc->focusedNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }

#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    if (!keyEvent.isKeyUp())
        loader()->resetMultipleFormSubmissionProtection();
#endif

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.

    return result;
}

FrameWinClient* FrameWin::client() const
{
    return m_client;
}

} // namespace WebCore
