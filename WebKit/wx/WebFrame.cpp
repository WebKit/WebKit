/*
 * Copyright (C) 2007 Kevin Ollivier  All rights reserved.
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
#include "CString.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLFrameOwnerElement.h"
#include "markup.h"
#include "Page.h"
#include "PlatformString.h"
#include "RenderTreeAsText.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptValue.h"
#include "SubstituteData.h"
#include "TextEncoding.h"

#include "JSDOMBinding.h"
#include <runtime/JSValue.h>
#include <runtime/UString.h>

#include "EditorClientWx.h"
#include "FrameLoaderClientWx.h"

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebFrame.h"
#include "WebView.h"
#include "WebFramePrivate.h"
#include "WebViewPrivate.h"

#include <wx/defs.h>
#include <wx/dcbuffer.h>

// Match Safari's min/max zoom sizes by default
#define MinimumTextSizeMultiplier       0.5f
#define MaximumTextSizeMultiplier       3.0f
#define TextSizeMultiplierRatio         1.2f

wxWebFrame::wxWebFrame(wxWebView* container, wxWebFrame* parent, WebViewFrameData* data) :
    m_textMagnifier(1.0),
    m_isEditable(false),
    m_isInitialized(false),
    m_beingDestroyed(false)
{

    m_impl = new WebFramePrivate();
 
    WebCore::HTMLFrameOwnerElement* parentFrame = 0;
    
    if (data) {
        parentFrame = data->ownerElement;
    }
    
    WebCore::FrameLoaderClientWx* loaderClient = new WebCore::FrameLoaderClientWx();
    RefPtr<WebCore::Frame> newFrame = WebCore::Frame::create(container->m_impl->page, parentFrame, loaderClient);

    m_impl->frame = newFrame.get();

    loaderClient->setFrame(this);
    loaderClient->setWebView(container);
    
    if (data && data->ownerElement)
        m_impl->frame->ref();

    m_impl->frame->init();
        
    m_isInitialized = true;
}

wxWebFrame::~wxWebFrame()
{
    if (m_impl)
        delete m_impl;
}

WebCore::Frame* wxWebFrame::GetFrame()
{
    if (m_impl)
        return m_impl->frame;
        
    return 0;
}

void wxWebFrame::Stop()
{
    if (m_impl->frame && m_impl->frame->loader())
        m_impl->frame->loader()->stop();
}

void wxWebFrame::Reload()
{
    if (m_impl->frame && m_impl->frame->loader())
        m_impl->frame->loader()->reload();
}

wxString wxWebFrame::GetPageSource()
{
    if (m_impl->frame) {
        if (m_impl->frame->view() && m_impl->frame->view()->layoutPending())
            m_impl->frame->view()->layout();
    
        WebCore::Document* doc = m_impl->frame->document();
        
        if (doc) {
            wxString source = createMarkup(doc);
            return source;
        }
    }
    return wxEmptyString;
}

void wxWebFrame::SetPageSource(const wxString& source, const wxString& baseUrl)
{
    if (m_impl->frame && m_impl->frame->loader()) {
        WebCore::KURL url(WebCore::KURL(), static_cast<const char*>(baseUrl.mb_str(wxConvUTF8)));

        wxCharBuffer charBuffer(source.mb_str(wxConvUTF8));
        const char* contents = charBuffer;

        WTF::PassRefPtr<WebCore::SharedBuffer> sharedBuffer = WebCore::SharedBuffer::create(contents, strlen(contents));
        WebCore::SubstituteData substituteData(sharedBuffer, WebCore::String("text/html"), WebCore::String("UTF-8"), WebCore::blankURL(), url);

        m_impl->frame->loader()->stop();
        m_impl->frame->loader()->load(WebCore::ResourceRequest(url), substituteData, false);
    }
}

wxString wxWebFrame::GetInnerText()
{
    if (m_impl->frame->view() && m_impl->frame->view()->layoutPending())
        m_impl->frame->view()->layout();
        
    WebCore::Element *documentElement = m_impl->frame->document()->documentElement();
    return documentElement->innerText();
}

wxString wxWebFrame::GetAsMarkup()
{
    if (!m_impl->frame || !m_impl->frame->document())
        return wxEmptyString;

    return createMarkup(m_impl->frame->document());
}

wxString wxWebFrame::GetExternalRepresentation()
{
    if (m_impl->frame->view() && m_impl->frame->view()->layoutPending())
        m_impl->frame->view()->layout();

    return externalRepresentation(m_impl->frame->contentRenderer());
}

wxString wxWebFrame::RunScript(const wxString& javascript)
{
    wxString returnValue = wxEmptyString;
    if (m_impl->frame) {
        JSC::JSValue result = m_impl->frame->script()->executeScript(javascript, true).jsValue();
        if (result)
            returnValue = wxString(result.toString(m_impl->frame->script()->globalObject()->globalExec()).UTF8String().c_str(), wxConvUTF8);        
    }
    return returnValue;
}

bool wxWebFrame::FindString(const wxString& string, bool forward, bool caseSensitive, bool wrapSelection, bool startInSelection)
{
    if (m_impl->frame)
        return m_impl->frame->findString(string, forward, caseSensitive, wrapSelection, startInSelection);

    return false;
}

void wxWebFrame::LoadURL(const wxString& url)
{
    if (m_impl->frame && m_impl->frame->loader()) {
        WebCore::KURL kurl = WebCore::KURL(WebCore::KURL(), static_cast<const char*>(url.mb_str(wxConvUTF8)), WebCore::UTF8Encoding());
        // NB: This is an ugly fix, but CURL won't load sub-resources if the
        // protocol is omitted; sadly, it will not emit an error, either, so
        // there's no way for us to catch this problem the correct way yet.
        if (kurl.protocol().isEmpty()) {
            // is it a file on disk?
            if (wxFileExists(url)) {
                kurl.setProtocol("file");
                kurl.setPath("//" + kurl.path());
            }
            else {
                kurl.setProtocol("http");
                kurl.setPath("//" + kurl.path());
            }
        }
        m_impl->frame->loader()->load(kurl, false);
    }
}

bool wxWebFrame::GoBack()
{
    if (m_impl->frame && m_impl->frame->page())
        return m_impl->frame->page()->goBack();

    return false;
}

bool wxWebFrame::GoForward()
{
    if (m_impl->frame && m_impl->frame->page())
        return m_impl->frame->page()->goForward();

    return false;
}

bool wxWebFrame::CanGoBack()
{
    if (m_impl->frame && m_impl->frame->page() && m_impl->frame->page()->backForwardList())
        return m_impl->frame->page()->backForwardList()->backItem() != NULL;

    return false;
}

bool wxWebFrame::CanGoForward()
{
    if (m_impl->frame && m_impl->frame->page() && m_impl->frame->page()->backForwardList())
        return m_impl->frame->page()->backForwardList()->forwardItem() != NULL;

    return false;
}

void wxWebFrame::Undo()
{
    if (m_impl->frame && m_impl->frame->editor() && CanUndo())
        return m_impl->frame->editor()->undo();
}

void wxWebFrame::Redo()
{
    if (m_impl->frame && m_impl->frame->editor() && CanRedo())
        return m_impl->frame->editor()->redo();
}

bool wxWebFrame::CanUndo()
{
    if (m_impl->frame && m_impl->frame->editor())
        return m_impl->frame->editor()->canUndo();

    return false;
}

bool wxWebFrame::CanRedo()
{
    if (m_impl->frame && m_impl->frame->editor())
        return m_impl->frame->editor()->canRedo();

    return false;
}

bool wxWebFrame::CanIncreaseTextSize() const
{
    if (m_impl->frame) {
        if (m_textMagnifier*TextSizeMultiplierRatio <= MaximumTextSizeMultiplier)
            return true;
    }
    return false;
}

void wxWebFrame::IncreaseTextSize()
{
    if (CanIncreaseTextSize()) {
        m_textMagnifier = m_textMagnifier*TextSizeMultiplierRatio;
        m_impl->frame->setZoomFactor(m_textMagnifier, true);
    }
}

bool wxWebFrame::CanDecreaseTextSize() const
{
    if (m_impl->frame) {
        if (m_textMagnifier/TextSizeMultiplierRatio >= MinimumTextSizeMultiplier)
            return true;
    }
    return false;
}

void wxWebFrame::DecreaseTextSize()
{        
    if (CanDecreaseTextSize()) {
        m_textMagnifier = m_textMagnifier/TextSizeMultiplierRatio;
        m_impl->frame->setZoomFactor(m_textMagnifier, true);
    }
}

void wxWebFrame::ResetTextSize()
{
    m_textMagnifier = 1.0;
    if (m_impl->frame)
        m_impl->frame->setZoomFactor(m_textMagnifier, true);
}

void wxWebFrame::MakeEditable(bool enable)
{
    m_isEditable = enable;
}



bool wxWebFrame::CanCopy()
{
    if (m_impl->frame && m_impl->frame->view())
        return (m_impl->frame->editor()->canCopy() || m_impl->frame->editor()->canDHTMLCopy());

    return false;
}

void wxWebFrame::Copy()
{
    if (CanCopy())
        m_impl->frame->editor()->copy();
}

bool wxWebFrame::CanCut()
{
    if (m_impl->frame && m_impl->frame->view())
        return (m_impl->frame->editor()->canCut() || m_impl->frame->editor()->canDHTMLCut());

    return false;
}

void wxWebFrame::Cut()
{
    if (CanCut())
        m_impl->frame->editor()->cut();
}

bool wxWebFrame::CanPaste()
{
    if (m_impl->frame && m_impl->frame->view())
        return (m_impl->frame->editor()->canPaste() || m_impl->frame->editor()->canDHTMLPaste());

    return false;
}

void wxWebFrame::Paste()
{
    if (CanPaste())
        m_impl->frame->editor()->paste();

}

wxWebViewDOMElementInfo wxWebFrame::HitTest(const wxPoint& pos) const
{
    wxWebViewDOMElementInfo domInfo;

    if (m_impl->frame->view()) {
        WebCore::HitTestResult result = m_impl->frame->eventHandler()->hitTestResultAtPoint(m_impl->frame->view()->windowToContents(pos), false);
        if (result.innerNode()) {
            domInfo.SetLink(result.absoluteLinkURL().string());
            domInfo.SetText(result.textContent());
            domInfo.SetImageSrc(result.absoluteImageURL().string());
            domInfo.SetSelected(result.isSelected());
        }
    }

    return domInfo;
}

bool wxWebFrame::ShouldClose() const
{
    if (m_impl->frame)
        return m_impl->frame->shouldClose();

    return true;
}
