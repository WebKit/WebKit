/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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

#include <WebCore/ScriptController.h>

#include "WebFrame.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClientHaiku.h"
#include "Element.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClientHaiku.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ProgressTrackerHaiku.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "wtf/URL.h"
#include "WebFramePrivate.h"
#include "WebPage.h"
#include "markup.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

#include <Entry.h>
#include <String.h>

static const float kMinimumZoomFactorMultiplier = 0.5;
static const float kMaximumZoomFactorMultiplier = 3;
static const float kZoomFactorMultiplierRatio = 1.1;

using namespace WebCore;

BWebFrame::BWebFrame(BWebPage* webPage, BWebFrame* parentFrame,
        WebFramePrivate* data)
    : fZoomFactor(1.0)
    , fIsEditable(false)
    , fTitle(0)
    , fData(data)
{
    if (!parentFrame) {
        // No parent, we are creating the main BWebFrame.
        // mainframe is already created in WebCore::Page, just use it.
        fData->frame = &webPage->page()->mainFrame();
		FrameLoaderClientHaiku& client
			= static_cast<FrameLoaderClientHaiku&>(data->frame->loader().client());
		client.setFrame(this);
        fData->frame->init();
        fData->frame->tree().setName(fData->name);
    } else {
        // Will be initialized in BWebFrame::AddChild
    }
}

BWebFrame::~BWebFrame()
{
    delete fData;
}


void BWebFrame::SetListener(const BMessenger& listener)
{
	FrameLoaderClientHaiku& client
		= static_cast<FrameLoaderClientHaiku&>(fData->frame->loader().client());
    client.setDispatchTarget(listener);
}

void BWebFrame::LoadURL(BString urlString)
{
    WTF::URL url;
    if (BEntry(urlString.String()).Exists()) {
        url.setProtocol("file");
        url.setPath(String(urlString));
    } else
		url = WTF::URL(WTF::URL(), urlString.Trim());

	if (!url.isValid()) {
        BString fixedUrl("http://");
        fixedUrl << urlString.Trim();
		url = WTF::URL(WTF::URL(), fixedUrl);
	}
    LoadURL(url);
}

void BWebFrame::LoadURL(WTF::URL url)
{
	if (url.isEmpty())
		return;

    if (!fData->frame)
        return;

    fData->requestedURL = url.string();

    WebCore::ResourceRequest req(url);
    fData->frame->loader().load(WebCore::FrameLoadRequest(*fData->frame, req));
}

void BWebFrame::StopLoading()
{
    if (fData->frame)
        fData->frame->loader().stop();
}

void BWebFrame::Reload()
{
    if (fData->frame)
        fData->frame->loader().reload();
}

BString BWebFrame::URL() const
{
    if (fData->frame->document() == NULL)
        return "";
    return fData->frame->document()->url().string();
}

BString BWebFrame::RequestedURL() const
{
    return fData->requestedURL;
}

bool BWebFrame::CanCopy() const
{
    if (fData->frame && fData->frame->view())
        return fData->frame->editor().canCopy() || fData->frame->editor().canDHTMLCopy();

    return false;
}

bool BWebFrame::CanCut() const
{
    if (fData->frame && fData->frame->view())
        return fData->frame->editor().canCut() || fData->frame->editor().canDHTMLCut();

    return false;
}

bool BWebFrame::CanPaste() const
{
    if (fData->frame && fData->frame->view())
        return fData->frame->editor().canPaste() || fData->frame->editor().canDHTMLPaste();

    return false;
}

void BWebFrame::Copy()
{
    if (CanCopy())
        fData->frame->editor().copy();
}

void BWebFrame::Cut()
{
    if (CanCut())
        fData->frame->editor().cut();
}

void BWebFrame::Paste()
{
    if (CanPaste())
        fData->frame->editor().paste();
}

bool BWebFrame::CanUndo() const
{
    if (fData->frame)
        return fData->frame->editor().canUndo();

    return false;
}

bool BWebFrame::CanRedo() const
{
    if (fData->frame)
        return fData->frame->editor().canRedo();

    return false;
}

void BWebFrame::Undo()
{
    if (CanUndo())
        return fData->frame->editor().undo();
}

void BWebFrame::Redo()
{
    if (CanRedo())
        return fData->frame->editor().redo();
}

bool BWebFrame::AllowsScrolling() const
{
    if (fData->frame && fData->frame->view())
        return fData->frame->view()->canHaveScrollbars();

    return false;
}

void BWebFrame::SetAllowsScrolling(bool flag)
{
    if (fData->frame && fData->frame->view())
        fData->frame->view()->setCanHaveScrollbars(flag);
}

BPoint BWebFrame::ScrollPosition()
{
    return fData->frame->view()->scrollPosition();
}

/*!
    Returns the frame's content as HTML, enclosed in HTML and BODY tags.
*/
BString BWebFrame::FrameSource() const
{
    if (fData->frame) {
        WebCore::Document* document = fData->frame->document();

        if (document)
            return serializeFragment(*document, SerializedNodes::SubtreeIncludingNode);
    }

    return BString();
}

void BWebFrame::SetFrameSource(const BString& /*source*/)
{
    // FIXME: see QWebFrame::setHtml/setContent
}

void BWebFrame::SetTransparent(bool transparent)
{
    if (fData->frame && fData->frame->view())
        fData->frame->view()->setTransparent(transparent);
}

bool BWebFrame::IsTransparent() const
{
    if (fData->frame && fData->frame->view())
        return fData->frame->view()->isTransparent();

    return false;
}

BString BWebFrame::InnerText() const
{
    WebCore::Element *documentElement = fData->frame->document()->documentElement();

    if (!documentElement)
        return String();

    return documentElement->innerText();
}

BString BWebFrame::AsMarkup() const
{
    if (!fData->frame->document())
        return BString();

    return serializeFragment(*fData->frame->document(), SerializedNodes::SubtreeIncludingNode);
}

BString BWebFrame::ExternalRepresentation() const
{
    return externalRepresentation(fData->frame);
}

bool BWebFrame::FindString(const BString& string, WebCore::FindOptions options)
{
    if (fData->page)
        return fData->page->findString(string, options);
    return false;
}

bool BWebFrame::CanIncreaseZoomFactor() const
{
    if (fData->frame) {
        if (fZoomFactor * kZoomFactorMultiplierRatio <= kMaximumZoomFactorMultiplier)
            return true;
    }

    return false;
}

bool BWebFrame::CanDecreaseZoomFactor() const
{
    if (fData->frame)
        return fZoomFactor / kZoomFactorMultiplierRatio >= kMinimumZoomFactorMultiplier;

    return false;
}

void BWebFrame::IncreaseZoomFactor(bool textOnly)
{
    if (CanIncreaseZoomFactor()) {
        fZoomFactor = fZoomFactor * kZoomFactorMultiplierRatio;
        if (textOnly)
            fData->frame->setTextZoomFactor(fZoomFactor);
        else
            fData->frame->setPageAndTextZoomFactors(fZoomFactor, fZoomFactor);
    }
}

void BWebFrame::DecreaseZoomFactor(bool textOnly)
{
    if (CanDecreaseZoomFactor()) {
        fZoomFactor = fZoomFactor / kZoomFactorMultiplierRatio;
        if (textOnly)
            fData->frame->setTextZoomFactor(fZoomFactor);
        else
            fData->frame->setPageAndTextZoomFactors(fZoomFactor, fZoomFactor);
    }
}

void BWebFrame::ResetZoomFactor()
{
    if (fZoomFactor == 1)
        return;

    fZoomFactor = 1;

    if (fData->frame)
        fData->frame->setPageAndTextZoomFactors(fZoomFactor, fZoomFactor);
}

void BWebFrame::SetEditable(bool editable)
{
    fIsEditable = editable;
}

bool BWebFrame::IsEditable() const
{
    return fIsEditable;
}

void BWebFrame::SetTitle(const BString& title)
{
	if (fTitle == title)
	    return;

    fTitle = title;
}

const BString& BWebFrame::Title() const
{
    return fTitle;
}

// #pragma mark - private


const char* BWebFrame::Name() const
{
    fName = WTF::String(Frame()->tree().uniqueName()).utf8().data();
    return fName.String();
}


JSGlobalContextRef BWebFrame::GlobalContext() const
{
	return toGlobalRef(Frame()->script().globalObject(mainThreadNormalWorld()));
}


WebCore::Frame* BWebFrame::Frame() const
{
    return fData->frame;
}


BWebFrame* BWebFrame::AddChild(BWebPage* page, BString name,
    WebCore::HTMLFrameOwnerElement* ownerElement)
{
    WebFramePrivate* data = new WebFramePrivate(page->page());
    data->name = name;
    data->ownerElement = ownerElement;

    BWebFrame* frame = new(std::nothrow) BWebFrame(page, this, data);

    if (!frame)
        return nullptr;

    RefPtr<WebCore::Frame> coreFrame = WebCore::Frame::create(fData->page,
        ownerElement, makeUniqueRef<FrameLoaderClientHaiku>(page));
    // We don't keep the reference to the Frame, see WebFramePrivate.h.
    data->frame = coreFrame.get();
    FrameLoaderClientHaiku& client = static_cast<FrameLoaderClientHaiku&>(data->frame->loader().client());
    client.setFrame(frame);
    coreFrame->tree().setName(name.String());

    if (ownerElement)
        ownerElement->document().frame()->tree().appendChild(*coreFrame.get());
    data->frame->init();
    // TODO? evas_object_smart_member_add(frame, ewkFrame);

    // The creation of the frame may have run arbitrary JavaScript that removed
    // it from the page already.
    if (!coreFrame->page()) {
        return nullptr;
    }
    return frame;
}
