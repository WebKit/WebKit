/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include "Pasteboard.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "Frame.h"
#include "KURL.h"
#include "markup.h"
#include "NotImplemented.h"
#include <wtf/text/WTFString.h>

#include <wx/defs.h>
#include <wx/dataobj.h>
#include <wx/clipbrd.h>

namespace WebCore {

Pasteboard::Pasteboard()
{
}

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    if (wxTheClipboard->Open()) {
#if wxCHECK_VERSION(2, 9, 4)
        wxTheClipboard->SetData(new wxHTMLDataObject(createMarkup(selectedRange, 0, AnnotateForInterchange)));
#endif
        wxTheClipboard->SetData(new wxTextDataObject(frame->editor()->selectedText()));
        wxTheClipboard->Close();
    }
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData( new wxTextDataObject(text) );
        wxTheClipboard->Close();
    }
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

String Pasteboard::plainText(Frame* frame)
{
    if (wxTheClipboard->Open()) {
        if (wxTheClipboard->IsSupported(wxDF_TEXT)) {
            wxTextDataObject data;
            wxTheClipboard->GetData(data);
            wxTheClipboard->Close();
            return data.GetText();
        }
    }
    
    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    RefPtr<DocumentFragment> fragment = 0;
    if (wxTheClipboard->Open()) {
#if wxCHECK_VERSION(2, 9, 4)
        if (wxTheClipboard->IsSupported(wxDF_HTML)) {
            wxHTMLDataObject data;
            wxTheClipboard->GetData(data);
            chosePlainText = false;
            fragment = createFragmentFromMarkup(frame->document(), data.GetHTML(), "", DisallowScriptingAndPluginContentIfNeeded);
        } else
#endif
        {
            if (allowPlainText && wxTheClipboard->IsSupported( wxDF_TEXT )) {
                wxTextDataObject data;
                wxTheClipboard->GetData( data );
                chosePlainText = true;
                fragment = createFragmentFromText(context.get(), data.GetText());
            }
        }
        wxTheClipboard->Close();
    }
    if (fragment)
        return fragment.release();
    
    return 0;
}

void Pasteboard::writeURL(const KURL& url, const String&, Frame*)
{
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData( new wxTextDataObject( url.string() ) );
        wxTheClipboard->Close();
    }
}

void Pasteboard::writeClipboard(Clipboard*)
{
    notImplemented();
}

void Pasteboard::clear()
{
    wxTheClipboard->Clear();
}

void Pasteboard::writeImage(Node*, const KURL&, const String& title)
{
    notImplemented();
}
    
}
